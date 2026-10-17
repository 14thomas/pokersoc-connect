#pragma once

#include <string>

#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"
#include "domain/session/SessionMode.h"

namespace pokersoc::domain {
class Cashbox;
class TransactionLog;
class Player;
class BuyIn;
} // namespace pokersoc::domain

namespace pokersoc::persistence {
class AppStore;
}

namespace pokersoc::application {

/// Use case: buy a player in. GUI-agnostic — the console (and later the GUI)
/// both call this. The active SessionMode decides the amount (cash game: the
/// requested amount within limits; tournament: a fixed price); the service then
/// makes change, persists, applies the cashbox movements and logs it.
class BuyInService {
public:
    struct Result {
        bool success = false;
        std::string message;
        const domain::BuyIn* transaction = nullptr; // owned by the log
        domain::DenominationCount change;            // cash handed back
        domain::Money buyInAmount;                   // chip value bought
        bool isRebuy = false;
    };

    BuyInService(domain::SessionMode& mode,
                 domain::Cashbox& cashbox,
                 domain::TransactionLog& log,
                 persistence::AppStore& store)
        : mode_(mode), cashbox_(cashbox), log_(log), store_(store) {}

    /// Ask the mode what this buy-in would cost / whether it is allowed, without
    /// changing anything. Lets the UI show the price before cash is tendered.
    domain::BuyInQuote preview(const domain::Player& player,
                               domain::Money requestedAmount) const;

    /// Perform the buy-in. `cashReceived` is the physical cash tendered;
    /// `requestedAmount` is the chip value the player asked to buy (used by cash
    /// games, ignored by tournaments which use their fixed price).
    Result buyIn(const domain::Player& player,
                 const domain::DenominationCount& cashReceived,
                 domain::Money requestedAmount);

    /// How many buy-ins (incl. rebuys) `playerId` has already made this session.
    int existingBuyInCount(const std::string& playerId) const;

private:
    domain::SessionMode& mode_;
    domain::Cashbox& cashbox_;
    domain::TransactionLog& log_;
    persistence::AppStore& store_;
};

} // namespace pokersoc::application
