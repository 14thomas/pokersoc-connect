#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "domain/cashbox/Cashbox.h"
#include "domain/food/ProductCatalog.h"
#include "domain/food/SaleLine.h"
#include "domain/player/Attendance.h"
#include "domain/player/PlayerDirectory.h"
#include "domain/session/Session.h"
#include "domain/transaction/TransactionLog.h"

namespace pokersoc::domain {
class DenominationTable;
class Player;
} // namespace pokersoc::domain

namespace pokersoc::persistence {
class AppStore;
}

namespace pokersoc::application {
class AdminService;
class BuyInService;
class CashOutService;
class CashboxService;
class FoodService;
class ImportExportService;
class Ledger;
class PlayerService;
class ReportService;
} // namespace pokersoc::application

namespace pokersoc::ui {

/// The console front end for one open session file. Owns the session's live
/// state (cashbox, log, players, catalog) and the services that operate on it;
/// every screen is a thin layer over those services.
///
/// `run()` returns when the user quits or switches to another session file
/// (new session / open another / restore), so `main` can loop over sessions.
class ConsoleApp {
public:
    struct Exit {
        bool quit = true;
        std::string nextSession; // set when !quit
    };

    ConsoleApp(std::string dbPath, const domain::DenominationTable& table);
    ~ConsoleApp();

    ConsoleApp(const ConsoleApp&) = delete;
    ConsoleApp& operator=(const ConsoleApp&) = delete;

    Exit run();

private:
    // ---- ConsoleApp.cpp: wiring, main menu, scan, buy-in, cash-out ----
    bool chooseMode();
    void wireServices();
    void offerOpeningFloat();
    void printHeader();
    bool isTournament() const;
    const domain::Player* currentPlayer();
    void touchCurrentPlayer();
    void clearCurrentPlayer();
    /// The selected player, or prompt for a scan if none is selected.
    const domain::Player* requirePlayer();
    void scanPlayer();
    const domain::Player* registerNewPlayer(const std::string& id);
    void buyIn();
    void cashOut();

    // ---- ConsoleCashbox.cpp ----
    void cashboxMenu();
    void showCashbox();
    void addFloat();
    void addTip();
    void exchange();
    void lostChips();

    // ---- ConsoleFood.cpp ----
    void foodMenu();
    void sellFood();
    /// Build a basket from the catalog; nullopt if cancelled.
    std::optional<std::vector<domain::SaleLine>> pickFood(const std::string& title);
    void manageCatalog();
    void manageSlots();

    // ---- ConsolePlayers.cpp ----
    void playersMenu();
    void listPlayers(const std::vector<const domain::Player*>& players);
    void showPlayer(const domain::Player& player);
    void editPlayer(const std::string& id);
    void editMembership(const std::string& id);
    void setAgeStatus();
    void importPlayers();
    void exportPlayers();

    // ---- ConsoleAdmin.cpp ----
    bool confirmAdmin();
    void activityLog();
    void sessionInfo();
    void tournamentSettings();
    void cashGameSettings();
    void settingsMenu();
    void backupSession();
    void exportSessionPlayers();
    void catalogImportExport();
    void changePassword();

    std::string dbPath_;
    const domain::DenominationTable& table_;
    std::unique_ptr<persistence::AppStore> store_;

    domain::Cashbox cashbox_;
    domain::TransactionLog log_;
    domain::PlayerDirectory directory_;
    domain::Attendance attendance_;
    domain::ProductCatalog catalog_;
    std::unique_ptr<domain::Session> session_;

    std::unique_ptr<application::Ledger> ledger_;
    std::unique_ptr<application::PlayerService> players_;
    std::unique_ptr<application::BuyInService> buyIns_;
    std::unique_ptr<application::CashOutService> cashOuts_;
    std::unique_ptr<application::CashboxService> cashboxOps_;
    std::unique_ptr<application::FoodService> food_;
    std::unique_ptr<application::ReportService> reports_;
    std::unique_ptr<application::AdminService> admin_;
    std::unique_ptr<application::ImportExportService> files_;

    std::string currentPlayerId_;
    std::chrono::steady_clock::time_point currentPlayerSince_;
    std::optional<Exit> exit_; // set by a screen that wants to leave the session
};

} // namespace pokersoc::ui
