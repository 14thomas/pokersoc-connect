#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/food/SaleLine.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"

namespace pokersoc::domain {
class CashOut;
class Player;
class SessionMode;
} // namespace pokersoc::domain

namespace pokersoc::application {

class Ledger;

/// Round to the nearest 5c (Australian cash rounding: 1-2c down, 3-4c up).
domain::Money roundToFiveCents(domain::Money amount) noexcept;

/// Use case: cash a player out. GUI-agnostic — the console (and later the GUI)
/// both call this. The player hands back chips and may at the same time buy
/// food (taken out of the chips), tip the house, or add extra cash. The payout
///
///   chips - food - tip + extra cash   (rounded to 5c)
///
/// is paid from the cashbox using the fewest notes/coins it can, unless staff
/// specify the exact change to hand over.
class CashOutService {
public:
    struct Request {
        std::string playerId;
        domain::DenominationCount chips;
        domain::Money tip;
        domain::DenominationCount extraCash;
        std::vector<domain::SaleLine> food;
        /// Exact notes/coins to pay out instead of the computed change.
        std::optional<domain::DenominationCount> customChange;
    };

    /// The worked figures for a request, before anything is committed.
    struct Plan {
        bool ok = false;
        std::string message;
        domain::Money chipValue;
        domain::Money foodTotal;
        domain::Money tip;
        domain::Money extraCash;
        domain::Money payout;            // what the player receives, rounded to 5c
        domain::DenominationCount change; // the notes/coins making up the payout
    };

    struct Result {
        bool success = false;
        std::string message;
        const domain::CashOut* transaction = nullptr; // owned by the log
        domain::DenominationCount change;              // cash handed to the player
    };

    CashOutService(domain::SessionMode& mode, Ledger& ledger) : mode_(mode), ledger_(ledger) {}

    /// Work out the payout and change for a request without changing anything.
    /// `player` may be null for an unregistered id.
    Plan plan(const domain::Player* player, const Request& request) const;

    /// Plan and, if possible, commit the cash-out.
    Result cashOut(const domain::Player* player, const Request& request);

private:
    domain::SessionMode& mode_;
    Ledger& ledger_;
};

} // namespace pokersoc::application
