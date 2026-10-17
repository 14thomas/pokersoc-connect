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

/// What a player has already bought this session.
struct BuyInHistory {
    int count = 0; // number of buy-ins + rebuys so far
    Money total;   // their combined chip value
};

/// The outcome of asking a mode how a player's buy-in should be handled.
struct BuyInQuote {
    bool allowed = false;    // did the mode permit this buy-in?
    Money amount;            // chip value the player receives
    bool isRebuy = false;    // false = first buy-in, true = a later rebuy
    std::string label;       // human description, e.g. "ARC Member Rebuy"
    std::string message;     // reason when !allowed, or extra info
    /// Tournament buy-in split (prize + bounty + rake == amount). Cash games
    /// leave these at zero and the full amount is treated as chips.
    Money prize;
    Money bounty;
    Money rake;

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
    /// ignore it and use their fixed price. `history` is what the player has
    /// already bought this session (count 0 = first buy-in).
    virtual BuyInQuote quoteBuyIn(const Player& player,
                                  Money requested,
                                  const BuyInHistory& history) const = 0;

    /// Cash games pay chip value back 1:1 on demand; tournaments pay prizes, so
    /// free chip cash-out is disabled there until prizing is designed.
    virtual bool allowsChipCashOut() const noexcept = 0;

    /// Chips are worth cash only in a cash game, so only there can chips pay for
    /// food or be banked as tips when found lying around.
    virtual bool chipsHaveCashValue() const noexcept = 0;

    /// One-line human description of the mode's rules, for the UI header.
    virtual std::string rulesSummary() const = 0;
};

} // namespace pokersoc::domain
