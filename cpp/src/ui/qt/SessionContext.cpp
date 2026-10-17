#include "ui/qt/SessionContext.h"

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
#include "domain/session/CashGameMode.h"
#include "domain/session/TournamentMode.h"
#include "persistence/SqliteStore.h"

namespace pokersoc::ui::qt {

using namespace domain;

SessionContext::SessionContext(std::string dbPath, const DenominationTable& table,
                               SessionModeType newMode)
    : dbPath_(std::move(dbPath)),
      table_(table),
      store_(std::make_unique<persistence::SqliteStore>(dbPath_)),
      cashbox_(Cashbox::withStandardCurrency(table)) {
    store_->load(table_, cashbox_, log_);
    store_->loadPlayers(directory_);
    store_->loadAttendance(attendance_);
    store_->loadCatalog(catalog_);

    std::optional<SessionModeType> saved = store_->loadSessionModeType();
    isNew_ = !saved.has_value();
    if (isNew_) {
        store_->saveSessionMode(newMode);
        if (newMode == SessionModeType::Tournament) {
            store_->saveTournamentConfig(TournamentConfig{});
        } else {
            store_->saveCashGameConfig(CashGameConfig{});
        }
        saved = newMode;
    }
    if (*saved == SessionModeType::Tournament) {
        session_ = std::make_unique<Session>(
            std::make_unique<TournamentMode>(store_->loadTournamentConfig()));
    } else {
        session_ = std::make_unique<Session>(
            std::make_unique<CashGameMode>(store_->loadCashGameConfig()));
    }

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

SessionContext::~SessionContext() = default;

bool SessionContext::isTournament() const noexcept {
    return session_->modeType() == SessionModeType::Tournament;
}

TournamentMode& SessionContext::tournament() {
    return static_cast<TournamentMode&>(session_->mode());
}

CashGameMode& SessionContext::cashGame() {
    return static_cast<CashGameMode&>(session_->mode());
}

void SessionContext::saveTournamentConfig() {
    store_->saveTournamentConfig(tournament().config());
}

void SessionContext::saveCashGameConfig() {
    store_->saveCashGameConfig(cashGame().config());
}

} // namespace pokersoc::ui::qt
