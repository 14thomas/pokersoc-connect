#pragma once

#include <string>

#include "application/Ledger.h"

namespace pokersoc::domain {
class DenominationCount;
class SessionMode;
} // namespace pokersoc::domain

namespace pokersoc::application {

/// Use case: the money-desk operations that are not tied to a buy-in or
/// cash-out — topping up the float, banking tips, keeping lost chips and
/// swapping notes for coins. Everything goes through the ledger, so it is
/// persisted first and can be undone.
class CashboxService {
public:
    CashboxService(domain::SessionMode& mode, Ledger& ledger) : mode_(mode), ledger_(ledger) {}

    OperationResult addFloat(const domain::DenominationCount& cash);
    OperationResult addTip(const domain::DenominationCount& cash,
                           const std::string& playerId = "",
                           const std::string& note = "");
    /// Cash games only: chips found / left behind are kept as tips.
    OperationResult recordLostChips(const domain::DenominationCount& chips,
                                    const std::string& note = "");
    /// `given` leaves the cashbox, `received` comes in; totals must match.
    OperationResult exchange(const domain::DenominationCount& given,
                             const domain::DenominationCount& received);

private:
    domain::SessionMode& mode_;
    Ledger& ledger_;
};

} // namespace pokersoc::application
