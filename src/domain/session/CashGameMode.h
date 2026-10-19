#pragma once

#include <optional>

#include "domain/money/Money.h"
#include "domain/session/SessionMode.h"

namespace pokersoc::domain {

/// The tunable settings of a cash game: the buy-in range, plus a hard session
/// cap on how much one player may buy in for in total. Kept as a small struct
/// so it is easy to persist and pass around.
struct CashGameConfig {
    Money minBuyIn = Money::fromCents(0);         // $0 by default
    Money maxBuyIn = Money::fromCents(7500);      // $75 by default; 0 = no per-buy-in max
    Money sessionLimit = Money::fromCents(22500); // $225 by default; 0 = no cap

    bool hasMaxBuyIn() const noexcept { return maxBuyIn > Money{}; }
    bool hasSessionCap() const noexcept { return sessionLimit > Money{}; }

    /// How much more this player may buy in for. Zero if they are already at
    /// the cap. Unused when there is no cap.
    Money remaining(const BuyInHistory& history) const noexcept {
        if (!hasSessionCap()) return Money{};
        const Money left = sessionLimit - history.total;
        return left.isNegative() ? Money{} : left;
    }

    /// False once the player's session buy-ins have reached the cap.
    bool hasRoom(const BuyInHistory& history) const noexcept {
        return !hasSessionCap() || history.total < sessionLimit;
    }

    /// Tightest ceiling for this buy-in (per-buy-in max and/or remaining cap).
    /// Empty when neither bound applies.
    std::optional<Money> ceiling(const BuyInHistory& history) const noexcept {
        std::optional<Money> max;
        if (hasMaxBuyIn()) max = maxBuyIn;
        if (hasSessionCap()) {
            const Money room = remaining(history);
            if (!max || room < *max) max = room;
        }
        return max;
    }
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
                          const BuyInHistory& history) const override;

    bool allowsChipCashOut() const noexcept override { return true; }
    bool chipsHaveCashValue() const noexcept override { return true; }

    std::string rulesSummary() const override;

    const CashGameConfig& config() const noexcept { return config_; }
    CashGameConfig& config() noexcept { return config_; }
    Money minBuyIn() const noexcept { return config_.minBuyIn; }
    Money maxBuyIn() const noexcept { return config_.maxBuyIn; }

private:
    CashGameConfig config_;
};

} // namespace pokersoc::domain
