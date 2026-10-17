#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "application/AdminService.h"
#include "application/ImportExportService.h"
#include "application/Ledger.h"
#include "application/PlayerService.h"
#include "application/ReportService.h"
#include "domain/player/Player.h"
#include "domain/session/CashGameMode.h"
#include "domain/session/TournamentMode.h"
#include "domain/transaction/Transaction.h"
#include "persistence/AppStore.h"
#include "ui/console/ConsoleApp.h"
#include "ui/console/ConsoleIO.h"
#include "util/Time.h"

namespace pokersoc::ui {

using domain::Money;
using domain::TournamentConfig;
using domain::TournamentMode;
using domain::TournamentPricing;

namespace {

constexpr std::size_t kActivityPageSize = 30;

void editPrice(const std::string& label, std::int64_t& cents) {
    const Money current = Money::fromCents(cents);
    cents = askMoney("    " + label + " [" + current.toString() + "]: ", current)
                .value_or(current)
                .cents();
}

} // namespace

bool ConsoleApp::confirmAdmin() {
    const std::string attempt = ask("  Admin password: ");
    if (admin_->checkPassword(attempt)) {
        return true;
    }
    std::cout << "  Incorrect password.\n";
    return false;
}

// ---------------------------------------------------------------------------
// Activity log + undo

void ConsoleApp::activityLog() {
    std::string filter;
    const std::string choice =
        ask("  Show: Enter = everyone, p = current player, or a player id: ");
    if (choice == "p" || choice == "P") {
        filter = currentPlayerId_;
        if (filter.empty()) {
            std::cout << "  No player selected; showing everyone.\n";
        }
    } else {
        filter = choice;
    }

    const auto entries = reports_->activity(filter);
    if (entries.empty()) {
        std::cout << "  (no transactions)\n";
        return;
    }
    std::cout << '\n';
    const std::size_t shown = std::min(entries.size(), kActivityPageSize);
    for (std::size_t i = 0; i < shown; ++i) {
        const auto* transaction = entries[i];
        std::string who = transaction->playerId();
        if (const auto* player = players_->find(who); player && !player->name().empty()) {
            who = player->name() + " (" + who + ")";
        }
        std::cout << "  #" << std::left << std::setw(5) << transaction->id()
                  << util::utcToLocal(transaction->time()) << "  " << std::setw(11)
                  << domain::transactionTypeName(transaction->type()) << std::right
                  << (who.empty() ? "" : who + ": ") << transaction->describe() << '\n';
    }
    if (entries.size() > shown) {
        std::cout << "  ... " << (entries.size() - shown) << " older transaction(s) not shown.\n";
    }

    const int id = askInt("\n  Undo/delete a transaction? Enter its # (Enter = back): ", 0);
    if (id <= 0) return;
    const auto* transaction = log_.find(id);
    if (!transaction) {
        std::cout << "  No transaction #" << id << ".\n";
        return;
    }
    std::cout << "  #" << id << ": " << transaction->describe() << '\n';
    if (!confirmAdmin()) return;
    if (!askYesNo("  Reverse this transaction? (y/n) [n]: ", false)) return;
    const auto result = ledger_->undo(id);
    std::cout << "  " << result.message << '\n';
}

// ---------------------------------------------------------------------------
// Session / tournament info

void ConsoleApp::sessionInfo() {
    std::cout << "\n  " << session_->modeName() << " session: " << dbPath_ << '\n'
              << "  " << session_->mode().rulesSummary() << '\n';

    const auto summary = reports_->summary();
    std::cout << "  Players scanned in: " << summary.uniquePlayers << " (" << summary.scans
              << " scans), known players: " << players_->count() << '\n';

    if (isTournament()) {
        const auto stats = reports_->tournamentStats();
        std::cout << "\n  Entries:  " << stats.buyIns << " buy-ins, " << stats.rebuys
                  << " rebuys  (" << (stats.buyIns + stats.rebuys) << " total)\n"
                  << "  Collected: " << stats.collected.toString() << '\n';
        if (!stats.tiers.empty()) {
            std::cout << "\n  " << std::left << std::setw(18) << "Tier" << std::setw(10)
                      << "Buy-ins" << "Rebuys\n";
            for (const auto& tier : stats.tiers) {
                std::cout << "  " << std::setw(18) << tier.tier << std::setw(10) << tier.buyIns
                          << tier.rebuys << '\n';
            }
            std::cout << std::right;
        }
    }

    std::cout << "\n  Cash in box " << summary.cashInBox.toString() << "; float "
              << summary.floatTotal.toString() << "; buy-ins " << summary.buyInTotal.toString()
              << "; cashed out " << summary.cashOutTotal.toString() << "; tips "
              << summary.tipsTotal.toString() << "; food " << summary.foodSalesTotal.toString()
              << '\n';
}

// ---------------------------------------------------------------------------
// Mode settings

void ConsoleApp::tournamentSettings() {
    auto& mode = static_cast<TournamentMode&>(session_->mode());
    while (!inputClosed()) {
        TournamentConfig& config = mode.config();
        const std::string cap = config.rebuyCap <= 0 ? "unlimited" : std::to_string(config.rebuyCap);
        std::cout << "\n--- Tournament settings ---\n"
                  << "  Pricing: " << config.pricingSummary() << "; rebuy cap: " << cap << '\n'
                  << "  1) Rebuy cap\n"
                  << "  2) Pricing structure\n"
                  << "  3) Prices\n"
                  << "  0) Back\n";
        const std::string choice = ask("Choice: ", "0");

        if (choice == "1") {
            config.rebuyCap = std::max(0, askInt("  Rebuy cap (0 = unlimited) [" + cap + "]: ",
                                                 config.rebuyCap));
        } else if (choice == "2") {
            config.pricing = askInt("  1) ARC vs Non-ARC  2) Membership tiers: ",
                                    config.usesMembershipPricing() ? 2 : 1) == 2
                                 ? TournamentPricing::MembershipTiers
                                 : TournamentPricing::ArcVsNonArc;
            config.arcOnly = !config.usesMembershipPricing() &&
                             askYesNo(std::string("  ARC members only? (y/n) [") +
                                          (config.arcOnly ? "y" : "n") + "]: ",
                                      config.arcOnly);
        } else if (choice == "3") {
            if (config.usesMembershipPricing()) {
                editPrice("Bronze buy-in", config.bronzeBuyInCents);
                editPrice("Silver buy-in", config.silverBuyInCents);
                editPrice("Gold buy-in", config.goldBuyInCents);
                editPrice("Platinum buy-in", config.platinumBuyInCents);
                editPrice("Diamond buy-in", config.diamondBuyInCents);
                editPrice("Bronze rebuy", config.bronzeRebuyCents);
                editPrice("Silver rebuy", config.silverRebuyCents);
                editPrice("Gold rebuy", config.goldRebuyCents);
                editPrice("Platinum rebuy", config.platinumRebuyCents);
                editPrice("Diamond rebuy", config.diamondRebuyCents);
            } else {
                editPrice("ARC buy-in", config.arcBuyInCents);
                editPrice("Non-ARC buy-in", config.nonArcBuyInCents);
                editPrice("ARC rebuy", config.arcRebuyCents);
                editPrice("Non-ARC rebuy", config.nonArcRebuyCents);
            }
        } else if (choice == "0") {
            return;
        } else {
            continue;
        }
        store_->saveTournamentConfig(config);
        std::cout << "  Saved.\n";
    }
}

void ConsoleApp::cashGameSettings() {
    auto& config = static_cast<domain::CashGameMode&>(session_->mode()).config();
    domain::CashGameConfig edited = config;
    edited.minBuyIn = askMoney("  Minimum buy-in [" + config.minBuyIn.toString() + "]: ",
                               config.minBuyIn)
                          .value_or(config.minBuyIn);
    edited.maxBuyIn = askMoney("  Maximum buy-in [" + config.maxBuyIn.toString() + "]: ",
                               config.maxBuyIn)
                          .value_or(config.maxBuyIn);
    edited.sessionLimit = askMoney("  Session limit before a timeout, 0 = none [" +
                                       config.sessionLimit.toString() + "]: ",
                                   config.sessionLimit)
                              .value_or(config.sessionLimit);
    if (edited.maxBuyIn < edited.minBuyIn) {
        std::cout << "  The maximum must be at least the minimum; nothing changed.\n";
        return;
    }
    config = edited;
    store_->saveCashGameConfig(config);
    std::cout << "  Saved. " << session_->mode().rulesSummary() << '\n';
}

// ---------------------------------------------------------------------------
// Settings

void ConsoleApp::settingsMenu() {
    while (!exit_ && !inputClosed()) {
        std::cout << "\n--- Settings ---\n"
                  << "  1) Change admin password\n"
                  << "  2) Product catalog import / export\n"
                  << "  3) Export session players CSV\n"
                  << "  4) Back up this session\n"
                  << "  5) Open another session file (restore)\n"
                  << "  6) Start a new session\n"
                  << (isTournament() ? "  7) Tournament settings\n" : "  7) Cash game limits\n")
                  << "  8) Close session and quit\n"
                  << "  0) Back\n";
        const std::string choice = ask("Choice: ", "0");

        if (choice == "1") {
            changePassword();
        } else if (choice == "2") {
            catalogImportExport();
        } else if (choice == "3") {
            exportSessionPlayers();
        } else if (choice == "4") {
            backupSession();
        } else if (choice == "5") {
            const std::string path = ask("  Session file to open: ");
            if (path.empty()) continue;
            if (!std::filesystem::exists(path)) {
                std::cout << "  No such file.\n";
                continue;
            }
            if (askYesNo("  Close this session and open " + path + "? (y/n) [n]: ", false)) {
                exit_ = Exit{false, path};
            }
        } else if (choice == "6") {
            const std::string fallback = "Session-" + util::localNow("%Y-%m-%d-%H%M") + ".sqlite";
            const std::string path = ask("  New session file [" + fallback + "]: ", fallback);
            if (std::filesystem::exists(path)) {
                std::cout << "  " << path << " already exists; use option 5 to open it.\n";
                continue;
            }
            if (askYesNo("  Close this session and start " + path + "? (y/n) [n]: ", false)) {
                exit_ = Exit{false, path};
            }
        } else if (choice == "7") {
            isTournament() ? tournamentSettings() : cashGameSettings();
        } else if (choice == "8") {
            if (askYesNo("  Close the session and quit? (y/n) [n]: ", false)) {
                exit_ = Exit{};
            }
        } else if (choice == "0") {
            return;
        }
    }
}

void ConsoleApp::changePassword() {
    if (!confirmAdmin()) return;
    const std::string password = ask("  New password: ");
    if (password.empty()) {
        std::cout << "  Password unchanged.\n";
        return;
    }
    if (ask("  Repeat new password: ") != password) {
        std::cout << "  Passwords do not match; unchanged.\n";
        return;
    }
    admin_->setPassword(password);
    std::cout << "  Password changed.\n";
}

void ConsoleApp::backupSession() {
    const std::string fallback = "pokersoc-backup-" + util::localNow("%Y-%m-%d-%H%M") + ".db";
    const std::string path = ask("  Back up to [" + fallback + "]: ", fallback);
    try {
        store_->backupTo(path);
        std::cout << "  Backed up to " << path << ".\n";
    } catch (const std::exception& error) {
        std::cout << "  " << error.what() << '\n';
    }
}

void ConsoleApp::exportSessionPlayers() {
    const std::string fallback = "session_players_" + util::localNow("%Y-%m-%d_%H%M%S") + ".csv";
    const std::string path = ask("  Export to [" + fallback + "]: ", fallback);
    try {
        const auto count = files_->exportSessionPlayers(path);
        std::cout << "  Exported " << count << " player(s) to " << path << ".\n";
    } catch (const std::exception& error) {
        std::cout << "  Export failed: " << error.what() << '\n';
    }
}

void ConsoleApp::catalogImportExport() {
    const int choice = askInt("  1) Export catalog  2) Import catalog  0) Back: ", 0);
    try {
        if (choice == 1) {
            const std::string fallback = "catalog.json";
            const std::string path = ask("  Export to [" + fallback + "]: ", fallback);
            const auto count = files_->exportCatalog(path);
            std::cout << "  Exported " << count << " product(s) to " << path << ".\n";
        } else if (choice == 2) {
            const std::string path = ask("  JSON file to import: ");
            if (path.empty()) return;
            const bool merge =
                askInt("  1) Merge with current products  2) Replace them [1]: ", 1) != 2;
            const auto count = files_->importCatalog(path, merge);
            std::cout << "  Imported " << count << " product(s).\n";
        }
    } catch (const std::exception& error) {
        std::cout << "  " << error.what() << '\n';
    }
}

} // namespace pokersoc::ui
