#pragma once

#include "domain/money/Money.h"
#include "domain/session/SessionMode.h"

namespace pokersoc::domain {

/// The tunable settings of a cash game: the buy-in range. Kept as a small struct
/// so it is easy to persist and pass around.
struct CashGameConfig {
    Money minBuyIn = Money::fromCents(1000);   // $10 by default
    Money maxBuyIn = Money::fromCents(50000);  // $500 by default
};

/// Cash game (a subclass of SessionMode). Chips are worth cash 1:1: a player
/// buys in for any amount within [min, max], and can cash their chips back out
/// for cash at any time.
class CashGameMode final : public SessionMode {
public:
    explicit CashGameMode(CashGameConfig config) : config_(config) {}
    CashGameMode(Money minBuyIn, Money maxBuyIn)
        : config_{minBuyIn, maxBuyIn} {}

    SessionModeType type() const noexcept override { return SessionModeType::CashGame; }
    std::string name() const override { return "Cash Game"; }

    BuyInQuote quoteBuyIn(const Player& player,
                          Money requested,
                          int existingBuyInCount) const override;

    bool allowsChipCashOut() const noexcept override { return true; }

    std::string rulesSummary() const override;

    const CashGameConfig& config() const noexcept { return config_; }
    Money minBuyIn() const noexcept { return config_.minBuyIn; }
    Money maxBuyIn() const noexcept { return config_.maxBuyIn; }

private:
    CashGameConfig config_;
};

} // namespace pokersoc::domain
