#pragma once

#include <memory>
#include <optional>
#include <string>

#include "domain/cashbox/Cashbox.h"
#include "domain/food/ProductCatalog.h"
#include "domain/player/Attendance.h"
#include "domain/player/PlayerDirectory.h"
#include "domain/session/Session.h"
#include "domain/transaction/TransactionLog.h"

namespace pokersoc::domain {
class CashGameMode;
class DenominationTable;
class TournamentMode;
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
class PrizeService;
class ReportService;
} // namespace pokersoc::application

namespace pokersoc::ui::qt {

/// One open session file: its live state (cashbox, log, players, catalog), the
/// active session mode and the application services over them. The Qt screens
/// only ever talk to these services.
class SessionContext {
public:
    /// Opens (or creates) `dbPath` and replays its history. A brand-new file
    /// takes `newMode`; an existing one keeps the mode it was created with.
    SessionContext(std::string dbPath, const domain::DenominationTable& table,
                   domain::SessionModeType newMode);
    ~SessionContext();

    SessionContext(const SessionContext&) = delete;
    SessionContext& operator=(const SessionContext&) = delete;

    const std::string& path() const noexcept { return dbPath_; }
    const domain::DenominationTable& table() const noexcept { return table_; }
    bool isNewSession() const noexcept { return isNew_; }

    persistence::AppStore& store() noexcept { return *store_; }
    domain::Cashbox& cashbox() noexcept { return cashbox_; }
    const domain::TransactionLog& log() const noexcept { return log_; }
    const domain::ProductCatalog& catalog() const noexcept { return catalog_; }
    domain::Session& session() noexcept { return *session_; }

    bool isTournament() const noexcept;
    domain::TournamentMode& tournament();
    domain::CashGameMode& cashGame();

    application::Ledger& ledger() noexcept { return *ledger_; }
    application::PlayerService& players() noexcept { return *players_; }
    application::BuyInService& buyIns() noexcept { return *buyIns_; }
    application::CashOutService& cashOuts() noexcept { return *cashOuts_; }
    application::CashboxService& cashboxOps() noexcept { return *cashboxOps_; }
    application::FoodService& food() noexcept { return *food_; }
    application::ReportService& reports() noexcept { return *reports_; }
    application::PrizeService& prizes() noexcept { return *prizes_; }
    application::AdminService& admin() noexcept { return *admin_; }
    application::ImportExportService& files() noexcept { return *files_; }

    void saveTournamentConfig();
    void saveCashGameConfig();

private:
    std::string dbPath_;
    const domain::DenominationTable& table_;
    std::unique_ptr<persistence::AppStore> store_;
    bool isNew_ = false;

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
    std::unique_ptr<application::PrizeService> prizes_;
    std::unique_ptr<application::AdminService> admin_;
    std::unique_ptr<application::ImportExportService> files_;
};

} // namespace pokersoc::ui::qt
