#include "ui/console/ConsoleApp.h"

#include <iostream>
#include <utility>

#include "application/AdminService.h"
#include "application/BuyInService.h"
#include "application/CashOutService.h"
#include "application/CashboxService.h"
#include "application/FoodService.h"
#include "application/ImportExportService.h"
#include "application/Ledger.h"
#include "application/PlayerService.h"
#include "application/ReportService.h"
#include "domain/money/DenominationTable.h"
#include "domain/player/Player.h"
#include "domain/session/CashGameMode.h"
#include "domain/session/TournamentMode.h"
#include "persistence/SqliteStore.h"
#include "ui/console/ConsoleIO.h"
#include "util/Time.h"

namespace pokersoc::ui {

using namespace domain;
using application::CashOutService;

namespace {

constexpr auto kPlayerTimeout = std::chrono::minutes(3);

std::string playerLabel(const Player& player) {
    std::string label = player.name().empty() ? "New Player" : player.name();
    return label + " (" + player.id() + ")";
}

std::string playerTags(const Player& player) {
    std::string tags;
    if (player.arcStatus() == ArcStatus::Arc) tags += "ARC";
    if (player.arcStatus() == ArcStatus::NonArc) tags += "Non-ARC";
    if (player.membership() != MembershipType::None) {
        if (!tags.empty()) tags += ", ";
        tags += membershipTypeLabel(player.membership());
    }
    return tags;
}

DenominationCount buildStandardFloat(const DenominationTable& table) {
    DenominationCount floatCount;
    const auto add = [&](std::int64_t cents, int qty) {
        if (const auto denomination = table.findCurrency(cents)) {
            floatCount.add(denomination, qty);
        }
    };
    add(5000, 2); add(2000, 5); add(1000, 10); add(500, 10);
    add(200, 20); add(100, 20); add(50, 20);
    add(20, 20); add(10, 20); add(5, 40);
    return floatCount;
}

} // namespace

ConsoleApp::ConsoleApp(std::string dbPath, const DenominationTable& table)
    : dbPath_(std::move(dbPath)),
      table_(table),
      store_(std::make_unique<persistence::SqliteStore>(dbPath_)),
      cashbox_(Cashbox::withStandardCurrency(table)) {
    // Players, cashbox, attendance and catalog are shared, mode-independent
    // state: rebuild them before we even know which mode this session runs.
    store_->load(table_, cashbox_, log_);
    store_->loadPlayers(directory_);
    store_->loadAttendance(attendance_);
    store_->loadCatalog(catalog_);
}

ConsoleApp::~ConsoleApp() = default;

ConsoleApp::Exit ConsoleApp::run() {
    const bool isNewSession = !store_->loadSessionModeType().has_value();
    if (!chooseMode()) {
        return Exit{};
    }
    wireServices();
    std::cout << '\n' << session_->mode().rulesSummary() << '\n';

    if (isNewSession) {
        const std::string csv = ask("\nImport players from a CSV file? (path, Enter = skip): ");
        if (!csv.empty()) {
            try {
                const auto result = files_->importPlayers(csv, true);
                std::cout << "  Imported " << (result.added + result.updated) << " player(s).\n";
            } catch (const std::exception& error) {
                std::cout << "  Could not import players: " << error.what()
                          << "\n  Continuing without import.\n";
            }
        }
    }
    offerOpeningFloat();

    while (!exit_ && !inputClosed()) {
        printHeader();
        std::cout << "  1) Scan player\n"
                  << "  2) Buy-in\n"
                  << "  3) Cash-out\n"
                  << "  4) Food & drink\n"
                  << "  5) Cashbox\n"
                  << "  6) Activity log\n"
                  << "  7) Players\n"
                  << (isTournament() ? "  8) Tournament settings\n" : "  8) Lost chips\n")
                  << (isTournament() ? "  9) Tournament info\n" : "  9) Session info\n")
                  << " 10) Settings\n"
                  << "  c) Clear current player\n"
                  << "  0) Quit\n";

        const std::string choice = ask("Choice: ");
        if (choice.empty()) continue;

        if (choice == "1") scanPlayer();
        else if (choice == "2") buyIn();
        else if (choice == "3") cashOut();
        else if (choice == "4") foodMenu();
        else if (choice == "5") cashboxMenu();
        else if (choice == "6") activityLog();
        else if (choice == "7") playersMenu();
        else if (choice == "8") isTournament() ? tournamentSettings() : lostChips();
        else if (choice == "9") sessionInfo();
        else if (choice == "10") settingsMenu();
        else if (choice == "c" || choice == "C") clearCurrentPlayer();
        else if (choice == "0" || choice == "q" || choice == "Q") {
            if (inputClosed() || askYesNo("  Close the session and quit? (y/n) [n]: ", false)) {
                exit_ = Exit{};
            }
        } else {
            std::cout << "  Unknown option.\n";
        }
    }
    return exit_.value_or(Exit{});
}

// ---------------------------------------------------------------------------
// Wiring

bool ConsoleApp::chooseMode() {
    if (const auto saved = store_->loadSessionModeType()) {
        if (*saved == SessionModeType::Tournament) {
            session_ = std::make_unique<Session>(
                std::make_unique<TournamentMode>(store_->loadTournamentConfig()));
        } else {
            session_ = std::make_unique<Session>(
                std::make_unique<CashGameMode>(store_->loadCashGameConfig()));
        }
        std::cout << "Opened " << session_->modeName() << " session '" << dbPath_ << "'.\n";
        return true;
    }

    std::cout << "New session '" << dbPath_ << "'. Choose a mode:\n"
              << "  1) Cash Game\n"
              << "  2) Tournament\n";
    const int choice = askInt("Mode [1]: ", 1);

    if (choice == 2) {
        TournamentConfig config;
        std::cout << "Tournament pricing:\n"
                  << "  1) ARC vs Non-ARC\n"
                  << "  2) Membership tiers\n";
        config.pricing = askInt("Pricing [1]: ", 1) == 2 ? TournamentPricing::MembershipTiers
                                                         : TournamentPricing::ArcVsNonArc;
        if (!config.usesMembershipPricing()) {
            config.arcOnly = askYesNo("ARC members only? (y/n) [n]: ", false);
        }
        config.rebuyCap = askInt("Rebuy cap (0 = unlimited) [0]: ", 0);
        if (inputClosed()) return false;
        store_->saveSessionMode(SessionModeType::Tournament);
        store_->saveTournamentConfig(config);
        session_ = std::make_unique<Session>(std::make_unique<TournamentMode>(std::move(config)));
        std::cout << "(Prices can be changed under Tournament settings.)\n";
        return true;
    }

    CashGameConfig config;
    config.minBuyIn = askMoney("Minimum buy-in [$10]: ", config.minBuyIn).value_or(config.minBuyIn);
    config.maxBuyIn = askMoney("Maximum buy-in [$500]: ", config.maxBuyIn).value_or(config.maxBuyIn);
    config.sessionLimit =
        askMoney("Session limit before a timeout is required (0 = none) [$500]: ",
                 config.sessionLimit)
            .value_or(config.sessionLimit);
    if (inputClosed()) return false;
    store_->saveSessionMode(SessionModeType::CashGame);
    store_->saveCashGameConfig(config);
    session_ = std::make_unique<Session>(std::make_unique<CashGameMode>(config));
    return true;
}

void ConsoleApp::wireServices() {
    SessionMode& mode = session_->mode();
    ledger_ = std::make_unique<application::Ledger>(cashbox_, log_, *store_);
    players_ = std::make_unique<application::PlayerService>(directory_, attendance_, *store_);
    buyIns_ = std::make_unique<application::BuyInService>(mode, *ledger_);
    cashOuts_ = std::make_unique<application::CashOutService>(mode, *ledger_);
    cashboxOps_ = std::make_unique<application::CashboxService>(mode, *ledger_);
    food_ = std::make_unique<application::FoodService>(catalog_, table_, mode, *ledger_, *store_);
    reports_ = std::make_unique<application::ReportService>(cashbox_, log_, attendance_);
    admin_ = std::make_unique<application::AdminService>(*store_);
    files_ = std::make_unique<application::ImportExportService>(*players_, *food_);
}

void ConsoleApp::offerOpeningFloat() {
    if (!cashbox_.total().isZero() || !log_.empty()) {
        return;
    }
    std::cout << "\nThe cashbox is empty.\n"
              << "  1) Count in the opening float\n"
              << "  2) Use the standard $428 float\n"
              << "  0) Skip\n";
    const int choice = askInt("Choice [2]: ", 2);
    if (choice == 1) {
        addFloat();
    } else if (choice == 2) {
        const auto result = cashboxOps_->addFloat(buildStandardFloat(table_));
        std::cout << "  " << result.message << '\n';
    }
}

bool ConsoleApp::isTournament() const {
    return session_->modeType() == SessionModeType::Tournament;
}

void ConsoleApp::printHeader() {
    std::cout << "\n=============== " << session_->modeName() << "  |  " << dbPath_
              << " ===============\n";
    std::cout << "Cashbox " << cashbox_.total().toString() << "  |  Players in: "
              << attendance_.uniquePlayerCount() << "  |  Transactions: " << log_.size() << '\n';

    if (const Player* player = currentPlayer()) {
        std::cout << "Player: " << playerLabel(*player);
        const std::string tags = playerTags(*player);
        if (!tags.empty()) std::cout << "  [" << tags << "]";
        if (player->isUnderage()) std::cout << "  ** UNDER 18 - no buy-in / cash-out **";
        std::cout << '\n';
    } else {
        std::cout << "Player: (none - scan a card)\n";
    }
}

// ---------------------------------------------------------------------------
// Current player

const Player* ConsoleApp::currentPlayer() {
    if (currentPlayerId_.empty()) {
        return nullptr;
    }
    if (std::chrono::steady_clock::now() - currentPlayerSince_ > kPlayerTimeout) {
        std::cout << "(Player " << currentPlayerId_ << " expired after 3 minutes.)\n";
        clearCurrentPlayer();
        return nullptr;
    }
    return players_->find(currentPlayerId_);
}

void ConsoleApp::touchCurrentPlayer() {
    currentPlayerSince_ = std::chrono::steady_clock::now();
}

void ConsoleApp::clearCurrentPlayer() {
    currentPlayerId_.clear();
}

const Player* ConsoleApp::requirePlayer() {
    if (const Player* player = currentPlayer()) {
        touchCurrentPlayer();
        return player;
    }
    scanPlayer();
    return currentPlayer();
}

void ConsoleApp::scanPlayer() {
    const std::string id = ask("  Scan / enter player id: ");
    if (id.empty()) {
        return;
    }

    const Player* player = players_->find(id);
    if (!player) {
        player = registerNewPlayer(id);
        if (!player) {
            std::cout << "  Cancelled.\n";
            return;
        }
    }

    players_->logAttendance(id); // logged even for under-18s, like the original
    currentPlayerId_ = id;
    touchCurrentPlayer();

    std::cout << "  Selected " << playerLabel(*player);
    const std::string tags = playerTags(*player);
    if (!tags.empty()) std::cout << " [" << tags << "]";
    std::cout << ".\n";
    if (player->isUnderage()) {
        std::cout << "  ** This player is UNDER 18: they cannot buy in or cash out. **\n";
    }
}

const Player* ConsoleApp::registerNewPlayer(const std::string& id) {
    std::cout << "  New player " << id << ". Check their ID before continuing.\n"
              << "  18+ if born on or before: " << util::adultCutoffDate() << '\n'
              << "    1) 18 or over\n"
              << "    2) Under 18\n"
              << "    0) Cancel\n";
    const int choice = askInt("  Choice: ", 0);
    if (choice != 1 && choice != 2) {
        return nullptr;
    }

    Player player(id, "New Player");
    if (choice == 2) {
        player.setUnderage(true);
    } else if (isTournament()) {
        // Membership is only collected for tournaments (it can set the price).
        std::cout << "  Membership: 0) None 1) Bronze 2) Silver 3) Gold 4) Platinum 5) Diamond\n";
        const int tier = askInt("  Membership [0]: ", 0);
        if (tier >= 1 && tier <= 5) {
            player.setMembership(static_cast<MembershipType>(tier));
        }
    }
    return &players_->save(std::move(player));
}

// ---------------------------------------------------------------------------
// Buy-in

void ConsoleApp::buyIn() {
    const Player* player = requirePlayer();
    if (!player) return;
    if (player->isUnderage()) {
        std::cout << "  " << playerLabel(*player) << " is under 18 and cannot buy in.\n";
        return;
    }
    const std::string id = player->id();

    Money requested; // ignored by tournaments (they use a fixed price)
    if (isTournament()) {
        const auto& config = static_cast<TournamentMode&>(session_->mode()).config();
        if (config.usesMembershipPricing() && player->membership() == MembershipType::None) {
            std::cout << "  Membership pricing is in use; select the player's membership.\n";
            editMembership(id);
        } else if (!config.usesMembershipPricing() && player->arcStatus() == ArcStatus::Unknown) {
            Player updated = *player;
            updated.setArcStatus(askYesNo("  Is the player an ARC member? (y/n) [n]: ", false)
                                     ? ArcStatus::Arc
                                     : ArcStatus::NonArc);
            players_->save(std::move(updated));
        }
        player = players_->find(id);
    } else {
        const auto& config = static_cast<CashGameMode&>(session_->mode()).config();
        const BuyInHistory history = buyIns_->history(id);
        if (history.count > 0) {
            std::cout << "  Session buy-ins so far: " << history.total.toString() << " ("
                      << history.count << ")\n";
        }
        const auto amount = askMoney("  Buy-in amount (" + config.minBuyIn.toString() + " - " +
                                     config.maxBuyIn.toString() + "): ");
        if (!amount) return;
        requested = *amount;
    }

    const BuyInQuote quote = buyIns_->preview(*player, requested);
    if (!quote.allowed) {
        std::cout << "  " << quote.message << '\n';
        return;
    }
    std::cout << "  " << quote.label << ": " << quote.amount.toString() << '\n';

    bool confirmed = false;
    if (quote.needsConfirmation) {
        std::cout << "  " << quote.confirmation << '\n';
        if (!askYesNo("  Timeout taken? (y/n) [n]: ", false)) {
            std::cout << "  Buy-in refused (no timeout).\n";
            return;
        }
        confirmed = true;
    }

    const auto cash = askCount("  Cash received [exact " + quote.amount.toString() + "]: ",
                               cashbox_.currency());
    if (!cash) return;
    const DenominationCount received =
        cash->empty() ? breakdown(quote.amount, cashbox_.currency()) : *cash;

    std::string notes;
    if (!isTournament()) {
        notes = ask("  Notes (optional): ");
    }

    const auto result = buyIns_->buyIn(*player, received, requested, confirmed, notes);
    std::cout << "  " << result.message << '\n';
    if (result.success) {
        std::cout << "  Give " << result.buyInAmount.toString() << " in chips.\n";
        if (!result.change.empty()) {
            std::cout << "  Change to hand back:\n";
            printCount(result.change);
        }
    }
    touchCurrentPlayer();
}

// ---------------------------------------------------------------------------
// Cash-out

void ConsoleApp::cashOut() {
    if (!session_->mode().allowsChipCashOut()) {
        std::cout << "  Tournament cash-out is coming soon (prizing is not set up yet).\n";
        return;
    }
    const Player* player = requirePlayer();
    if (!player) return;
    if (player->isUnderage()) {
        std::cout << "  " << playerLabel(*player) << " is under 18 and cannot cash out.\n";
        return;
    }

    CashOutService::Request request;
    request.playerId = player->id();

    const auto chips = askCount("  Chips handed in: ", table_.chips(), false);
    if (!chips) return;
    request.chips = *chips;
    std::cout << "  Chip value: " << request.chips.total().toString() << '\n';

    if (!catalog_.empty() && askYesNo("  Buy food from the chips? (y/n) [n]: ", false)) {
        const auto basket = pickFood("Food at cash-out");
        if (!basket) return;
        request.food = *basket;
    }

    const auto tip = askMoney("  Tip for the house [$0]: ", Money{});
    if (!tip) return;
    request.tip = *tip;

    const Money owed = request.chips.total() - saleLinesTotal(request.food) - request.tip;
    if (owed.isNegative()) {
        std::cout << "  Food and tip exceed the chips by " << (-owed).toString()
                  << "; the player must add cash.\n";
    }
    const auto extra = askCount("  Extra cash from the player [none]: ", cashbox_.currency());
    if (!extra) return;
    request.extraCash = *extra;

    while (true) {
        const CashOutService::Plan plan = cashOuts_->plan(player, request);
        if (!plan.ok) {
            std::cout << "  " << plan.message << '\n';
            return;
        }

        std::cout << "  ---------------------------------\n"
                  << "  Chips        " << plan.chipValue.toString() << '\n';
        if (!plan.foodTotal.isZero())
            std::cout << "  Food        -" << plan.foodTotal.toString() << "  ("
                      << describeSaleLines(request.food) << ")\n";
        if (!plan.tip.isZero()) std::cout << "  Tip         -" << plan.tip.toString() << '\n';
        if (!plan.extraCash.isZero())
            std::cout << "  Extra cash  +" << plan.extraCash.toString() << '\n';
        std::cout << "  Pay out      " << plan.payout.toString() << '\n';
        printCount(plan.change);

        const std::string answer =
            ask("  Pay this out? (y = yes, c = choose the change, n = cancel) [y]: ", "y");
        if (answer == "c" || answer == "C") {
            const auto custom = askCount("  Change to hand over: ", cashbox_.currency());
            if (!custom) return;
            request.customChange = *custom;
            continue;
        }
        if (answer != "y" && answer != "Y") {
            std::cout << "  Cancelled.\n";
            return;
        }
        break;
    }

    const auto result = cashOuts_->cashOut(player, request);
    std::cout << "  " << result.message << '\n';
    touchCurrentPlayer();
}

} // namespace pokersoc::ui
