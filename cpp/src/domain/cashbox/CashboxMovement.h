#pragma once

#include <memory>
#include <string>

#include "domain/cashbox/MovementReason.h"
#include "domain/money/Denomination.h"
#include "domain/money/Money.h"

namespace pokersoc::domain {

/// A single change to the cashbox: so many of one denomination moving in
/// (positive) or out (negative), with the reason and optional links.
struct CashboxMovement {
    std::shared_ptr<const Denomination> denomination;
    int deltaQty = 0; // + in, - out
    MovementReason reason = MovementReason::Adjust;
    std::string playerId; // optional
    std::string note;     // optional

    /// Signed cash value of this movement.
    Money value() const noexcept {
        return denomination ? denomination->value() * deltaQty : Money{};
    }
};

} // namespace pokersoc::domain
