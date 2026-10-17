#include <cctype>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "application/BuyInService.h"
#include "application/CashOutService.h"
#include "application/CashboxService.h"
#include "application/PlayerService.h"
#include "domain/cashbox/Cashbox.h"
#include "domain/money/Denomination.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/DenominationTable.h"
#include "domain/money/Money.h"
#include "domain/player/Player.h"
#include "domain/player/PlayerDirectory.h"
#include "domain/session/CashGameMode.h"
#include "domain/session/Session.h"
#include "domain/session/TournamentConfig.h"
#include "domain/session/TournamentMode.h"
#include "domain/transaction/TransactionLog.h"
#include "persistence/SqliteStore.h"

using namespace pokersoc::domain;
using pokersoc::application::BuyInService;
using pokersoc::application::CashboxService;
using pokersoc::application::CashOutService;
using pokersoc::application::PlayerService;
using pokersoc::persistence::AppStore;
using pokersoc::persistence::SqliteStore;

namespace {

using DenomPtr = std::shared_ptr<const Denomination>;

// --------------------------- input helpers ---------------------------------

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

/// Print a prompt and read a line. Returns false on end-of-input (so loops can
/// exit cleanly when input is piped / closed).
bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    std::cout.flush();
    return static_cast<bool>(std::getline(std::cin, out));
}

Money readMoney(const std::string& prompt, std::optional<Money> fallback = std::nullopt) {
    while (true) {
        std::string line;
        if (!readLine(prompt, line)) {
            return fallback.value_or(Money{});
        }
        line = trim(line);
        if (line.empty()) {
            if (fallback) return *fallback;
            std::cout << "  Please enter an amount.\n";
            continue;
        }
        if (line.front() == '$') line.erase(line.begin());
        try {
            return Money::fromDollars(std::stod(line));
        } catch (...) {
            std::cout << "  Please enter a dollar amount (e.g. 25 or 25.50).\n";
        }
    }
}

int readInt(const std::string& prompt, int fallback) {
    std::string line;
    if (!readLine(prompt, line)) return fallback;
    line = trim(line);
    if (line.empty()) return fallback;
    try {
        return std::stoi(line);
    } catch (...) {
        return fallback;
    }
}

bool readYesNo(const std::string& prompt, bool fallback) {
    std::string line;
    if (!readLine(prompt, line)) return fallback;
    line = trim(line);
    if (line.empty()) return fallback;
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(line.front())));
    return c == 'y';
}

// --------------------------- display helpers --------------------------------

void printDenomCount(const DenominationCount& count) {
    for (const auto& entry : count.entries()) {
        std::cout << "    " << std::left << std::setw(6) << entry.denomination->label()
                  << " x " << std::right << std::setw(3) << entry.qty << "   "
                  << (entry.denomination->value() * entry.qty).toString() << '\n';
    }
}

void printCashbox(const Cashbox& cashbox) {
    std::cout << "  Cashbox:\n";
    if (cashbox.stock().empty()) {
        std::cout << "    (empty)\n";
    } else {
        printDenomCount(cashbox.stock());
    }
    std::cout << "    total = " << cashbox.total().toString() << '\n';
}

void printPlayers(const PlayerService& players) {
    std::cout << "  Players (" << players.count() << "):\n";
    for (const auto* player : players.all()) {
        std::cout << "    " << player->describe() << '\n';
    }
}

void printSessionInfo(const Session& session, const Cashbox& cashbox,
                      const PlayerService& players, const TransactionLog& log) {
    std::cout << "  Mode: " << session.modeName() << '\n';
    std::cout << "  " << session.mode().rulesSummary() << '\n';
    std::cout << "  Cashbox total: " << cashbox.total().toString() << '\n';
    std::cout << "  Players: " << players.count() << ", transactions: " << log.size() << '\n';
}

// --------------------------- money helpers ----------------------------------

/// Greedily break `amount` into the given denominations (which must be sorted
/// high-to-low). Used to synthesise the physical cash a player tenders / the
/// chips they hand back from a single dollar figure typed at the console.
DenominationCount greedyBreakdown(const std::vector<DenomPtr>& denominations, Money amount) {
    DenominationCount count;
    std::int64_t remaining = amount.cents();
    for (const auto& denomination : denominations) {
        if (remaining <= 0) break;
        const std::int64_t face = denomination->cents();
        if (face <= 0) continue;
        const int qty = static_cast<int>(remaining / face);
        if (qty > 0) {
            count.add(denomination, qty);
            remaining -= static_cast<std::int64_t>(qty) * face;
        }
    }
    return count;
}

DenominationCount buildOpeningFloat(const DenominationTable& table) {
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

MembershipType membershipFromTier(int tier) {
    switch (tier) {
        case 1: return MembershipType::Bronze;
        case 2: return MembershipType::Silver;
        case 3: return MembershipType::Gold;
        case 4: return MembershipType::Platinum;
        case 5: return MembershipType::Diamond;
        default: return MembershipType::None;
    }
}

// --------------------------- commands ---------------------------------------

const Player* createPlayer(PlayerService& players, const std::string& id) {
    std::string name;
    readLine("    Name: ", name);
    Player player(id, trim(name));
    player.setArcStatus(readYesNo("    ARC member? (y/n) [n]: ", false) ? ArcStatus::Arc
                                                                        : ArcStatus::NonArc);
    const int tier = readInt(
        "    Membership 0=None 1=Bronze 2=Silver 3=Gold 4=Platinum 5=Diamond [0]: ", 0);
    player.setMembership(membershipFromTier(tier));
    return &players.save(std::move(player));
}

void doBuyIn(Session& session, BuyInService& buyIns, PlayerService& players,
             const Cashbox& cashbox) {
    std::string id;
    if (!readLine("  Player id: ", id)) return;
    id = trim(id);
    if (id.empty()) {
        std::cout << "  Cancelled.\n";
        return;
    }

    const Player* player = players.find(id);
    if (!player) {
        if (!readYesNo("  Unknown player. Create a quick profile? (y/n) [n]: ", false)) {
            std::cout << "  Cancelled.\n";
            return;
        }
        player = createPlayer(players, id);
    }

    Money requested; // ignored by tournaments (they use a fixed price)
    if (session.modeType() == SessionModeType::CashGame) {
        requested = readMoney("  Buy-in amount: ");
    }

    const BuyInQuote quote = buyIns.preview(*player, requested);
    if (!quote.allowed) {
        std::cout << "  " << quote.message << '\n';
        return;
    }
    std::cout << "  " << quote.label << ": " << quote.amount.toString()
              << (quote.isRebuy ? "  (rebuy)" : "") << '\n';

    const Money tendered = readMoney("  Cash tendered: ", quote.amount);
    const DenominationCount cashReceived = greedyBreakdown(cashbox.currency(), tendered);

    const auto result = buyIns.buyIn(*player, cashReceived, requested);
    std::cout << "  " << result.message << '\n';
    if (result.success && !result.change.empty()) {
        std::cout << "  Change given:\n";
        printDenomCount(result.change);
    }
}

void doCashOut(Session& session, CashOutService& cashOuts, PlayerService& players,
               const DenominationTable& table) {
    if (!session.mode().allowsChipCashOut()) {
        std::cout << "  Tournament payouts are prize-based and not configured yet.\n";
        return;
    }

    std::string id;
    if (!readLine("  Player id: ", id)) return;
    id = trim(id);
    if (id.empty()) {
        std::cout << "  Cancelled.\n";
        return;
    }
    if (!players.find(id)) {
        std::cout << "  Unknown player.\n";
        return;
    }

    const Money chipValue = readMoney("  Chip value to cash out: ");
    if (chipValue.isZero()) {
        std::cout << "  Nothing to cash out.\n";
        return;
    }
    const DenominationCount chips = greedyBreakdown(table.chips(), chipValue);
    const Money tip = readMoney("  Tip / lost chips kept by the house [$0]: ", Money{});

    const auto result = cashOuts.cashOut(id, chips, tip);
    std::cout << "  " << result.message << '\n';
    if (result.success && !result.change.empty()) {
        std::cout << "  Cash paid:\n";
        printDenomCount(result.change);
    }
}

void doPlayers(PlayerService& players) {
    printPlayers(players);
    if (!readYesNo("  Add a player? (y/n) [n]: ", false)) return;
    std::string id;
    readLine("    Player id: ", id);
    id = trim(id);
    if (id.empty()) return;
    createPlayer(players, id);
    std::cout << "  Saved.\n";
}

// --------------------------- mode selection ---------------------------------

std::unique_ptr<SessionMode> buildMode(AppStore& store) {
    if (const auto saved = store.loadSessionModeType()) {
        if (*saved == SessionModeType::Tournament) {
            std::cout << "Reopened Tournament session.\n";
            return std::make_unique<TournamentMode>(store.loadTournamentConfig());
        }
        std::cout << "Reopened Cash Game session.\n";
        return std::make_unique<CashGameMode>(store.loadCashGameConfig());
    }

    std::cout << "New session. Choose a mode:\n"
              << "  1) Cash Game\n"
              << "  2) Tournament\n";
    const int choice = readInt("Mode [1]: ", 1);

    if (choice == 2) {
        TournamentConfig config;
        std::cout << "Tournament pricing:\n"
                  << "  1) ARC vs Non-ARC\n"
                  << "  2) Membership tiers\n";
        config.pricing = readInt("Pricing [1]: ", 1) == 2 ? TournamentPricing::MembershipTiers
                                                          : TournamentPricing::ArcVsNonArc;
        config.arcOnly = readYesNo("ARC members only? (y/n) [n]: ", false);
        config.rebuyCap = readInt("Rebuy cap (0 = unlimited) [0]: ", 0);
        store.saveSessionMode(SessionModeType::Tournament);
        store.saveTournamentConfig(config);
        return std::make_unique<TournamentMode>(std::move(config));
    }

    CashGameConfig config;
    config.minBuyIn = readMoney("Minimum buy-in [$10]: ", Money::fromCents(1000));
    config.maxBuyIn = readMoney("Maximum buy-in [$500]: ", Money::fromCents(50000));
    store.saveSessionMode(SessionModeType::CashGame);
    store.saveCashGameConfig(config);
    return std::make_unique<CashGameMode>(config);
}

} // namespace

int main() {
    std::cout << "PokerSoc Connect (C++)\n\n";

    std::string dbPath;
    readLine("Session file [session.db]: ", dbPath);
    dbPath = trim(dbPath);
    if (dbPath.empty()) dbPath = "session.db";

    const DenominationTable table = DenominationTable::standardAustralian();
    SqliteStore store(dbPath);

    // Players and the cashbox are shared, mode-independent state: rebuild them
    // from the database before we even know which mode this session is.
    Cashbox cashbox = Cashbox::withStandardCurrency(table);
    TransactionLog log;
    PlayerDirectory directory;
    store.load(table, cashbox, log);
    store.loadPlayers(directory);

    Session session(buildMode(store));
    std::cout << '\n' << session.mode().rulesSummary() << '\n';

    PlayerService players(directory, store);
    CashboxService cashboxService(cashbox, store);
    CashOutService cashOutService(cashbox, log, store);
    BuyInService buyInService(session.mode(), cashbox, log, store);

    if (cashbox.total().isZero() &&
        readYesNo("\nCashbox is empty. Add a standard opening float? (y/n) [y]: ", true)) {
        cashboxService.addFloat(buildOpeningFloat(table));
        std::cout << "  Float added. Cashbox total " << cashbox.total().toString() << ".\n";
    }

    while (true) {
        std::cout << "\n=== " << session.modeName() << " ===\n"
                  << "  1) Buy-in\n"
                  << "  2) Cash-out\n"
                  << "  3) View cashbox\n"
                  << "  4) Players\n"
                  << "  5) Add float\n"
                  << "  6) Session info\n"
                  << "  0) Quit\n";

        std::string choice;
        if (!readLine("Choice: ", choice)) break;
        choice = trim(choice);

        if (choice == "0" || choice == "q" || choice == "Q") {
            break;
        } else if (choice == "1") {
            doBuyIn(session, buyInService, players, cashbox);
        } else if (choice == "2") {
            doCashOut(session, cashOutService, players, table);
        } else if (choice == "3") {
            printCashbox(cashbox);
        } else if (choice == "4") {
            doPlayers(players);
        } else if (choice == "5") {
            cashboxService.addFloat(buildOpeningFloat(table));
            std::cout << "  Float added. Cashbox total " << cashbox.total().toString() << ".\n";
        } else if (choice == "6") {
            printSessionInfo(session, cashbox, players, log);
        } else if (!choice.empty()) {
            std::cout << "  Unknown option.\n";
        }
    }

    std::cout << "\nGoodbye.\n";
    return 0;
}
