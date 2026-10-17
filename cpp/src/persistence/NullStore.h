#pragma once

#include <optional>

#include "persistence/AppStore.h"

namespace pokersoc::persistence {

/// A store that persists nothing. Handy for tests or running the app with no
/// database file.
class NullStore final : public AppStore {
public:
    void recordFloat(const domain::DenominationCount&) override {}
    void recordCashOut(const domain::CashOut&) override {}
    void recordBuyIn(const domain::BuyIn&) override {}
    void recordPlayer(const domain::Player&) override {}

    void saveSessionMode(domain::SessionModeType) override {}
    std::optional<domain::SessionModeType> loadSessionModeType() override { return std::nullopt; }
    void saveCashGameConfig(const domain::CashGameConfig&) override {}
    domain::CashGameConfig loadCashGameConfig() override { return {}; }
    void saveTournamentConfig(const domain::TournamentConfig&) override {}
    domain::TournamentConfig loadTournamentConfig() override { return {}; }

    void load(const domain::DenominationTable&, domain::Cashbox&, domain::TransactionLog&) override {}
    void loadPlayers(domain::PlayerDirectory&) override {}
};

} // namespace pokersoc::persistence
