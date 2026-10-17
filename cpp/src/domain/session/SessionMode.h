#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "domain/money/Money.h"

namespace pokersoc::domain {

class Player;

/// The two kinds of session this software runs. The entire difference between
/// them is how money changes hands (see SessionMode); players and the cashbox
/// are shared, mode-independent concepts.
enum class SessionModeType { CashGame, Tournament };

/// Stable text token for persistence / logs.
inline const char* sessionModeTypeName(SessionModeType type) noexcept {
    switch (type) {
        case SessionModeType::CashGame:   return "CASH_GAME";
        case SessionModeType::Tournament: return "TOURNAMENT";
    }
    return "CASH_GAME";
}

/// Inverse of sessionModeTypeName; unknown tokens fall back to CashGame.
inline SessionModeType sessionModeTypeFromName(std::string_view name) noexcept {
    if (name == "TOURNAMENT") return SessionModeType::Tournament;
    return SessionModeType::CashGame;
}

/// The outcome of asking a mode how a player's buy-in should be handled.
struct BuyInQuote {
    bool allowed = false;    // did the mode permit this buy-in?
    Money amount;            // chip value the player receives
    bool isRebuy = false;    // false = first buy-in, true = a later rebuy
    std::string label;       // human description, e.g. "ARC Member Rebuy"
    std::string message;     // reason when !allowed, or extra info

    /// Convenience for the common "not allowed" case.
    static BuyInQuote deny(std::string why, bool isRebuy = false) {
        BuyInQuote quote;
        quote.allowed = false;
        quote.isRebuy = isRebuy;
        quote.message = std::move(why);
        return quote;
    }
};

/// Abstract base (the parent class). A cash game and a tournament differ only in
/// how money moves, so that behaviour is what we make polymorphic. Subclasses:
/// CashGameMode and TournamentMode.
class SessionMode {
public:
    virtual ~SessionMode() = default;

    virtual SessionModeType type() const noexcept = 0;
    virtual std::string name() const = 0;

    /// Decide a buy-in for `player`. `requested` is the chip value the player
    /// asked to buy — cash games honour it (within their limits), tournaments
    /// ignore it and use their fixed price. `existingBuyInCount` is how many
    /// buy-ins the player has already made this session (0 = first buy-in).
    virtual BuyInQuote quoteBuyIn(const Player& player,
                                  Money requested,
                                  int existingBuyInCount) const = 0;

    /// Cash games pay chip value back 1:1 on demand; tournaments pay prizes, so
    /// free chip cash-out is disabled there until prizing is designed.
    virtual bool allowsChipCashOut() const noexcept = 0;

    /// One-line human description of the mode's rules, for the UI header.
    virtual std::string rulesSummary() const = 0;
};

} // namespace pokersoc::domain
