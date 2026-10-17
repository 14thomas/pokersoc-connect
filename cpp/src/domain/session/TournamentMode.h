#pragma once

#include <utility>

#include "domain/session/SessionMode.h"
#include "domain/session/TournamentConfig.h"

namespace pokersoc::domain {

/// Tournament (a subclass of SessionMode). Buy-ins are a fixed price decided by
/// the TournamentConfig (ARC status or membership tier), with an optional rebuy
/// cap. Payouts are prize-based, so free chip cash-out is disabled for now.
class TournamentMode final : public SessionMode {
public:
    explicit TournamentMode(TournamentConfig config) : config_(std::move(config)) {}

    SessionModeType type() const noexcept override { return SessionModeType::Tournament; }
    std::string name() const override { return "Tournament"; }

    BuyInQuote quoteBuyIn(const Player& player,
                          Money requested,
                          int existingBuyInCount) const override;

    bool allowsChipCashOut() const noexcept override { return false; }

    std::string rulesSummary() const override;

    const TournamentConfig& config() const noexcept { return config_; }
    TournamentConfig& config() noexcept { return config_; }

private:
    TournamentConfig config_;
};

} // namespace pokersoc::domain
