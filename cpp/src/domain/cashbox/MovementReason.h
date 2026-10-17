#pragma once

namespace pokersoc::domain {

enum class MovementReason {
    FloatAdd,  // Add money to the float
    BuyIn,     // Player buys in
    CashOut,   // Player cashes out
    ExtraCash, // Cash a player adds at cash-out
    Exchange,  // Like-for-like denomination swap
    Sale,      // Food sale paid in cash
    Tip,       // Tip received
    Adjust,    // Manual correction
    Undo       // Reversal of a previous movement
};

inline const char* movementReasonName(MovementReason reason) noexcept {
    switch (reason) {
        case MovementReason::FloatAdd:  return "FLOAT_ADD";
        case MovementReason::BuyIn:     return "BUYIN";
        case MovementReason::CashOut:   return "CASHOUT";
        case MovementReason::ExtraCash: return "EXTRA_CASH";
        case MovementReason::Exchange:  return "EXCHANGE";
        case MovementReason::Sale:      return "SALE";
        case MovementReason::Tip:       return "TIP";
        case MovementReason::Adjust:    return "ADJUST";
        case MovementReason::Undo:      return "UNDO";
    }
    return "?";
}

} // namespace pokersoc::domain
