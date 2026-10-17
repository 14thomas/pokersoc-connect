#include "domain/transaction/BuyIn.h"

#include <utility>

#include "domain/cashbox/Cashbox.h"

namespace pokersoc::domain {

BuyIn::BuyIn(std::string playerId,
             DenominationCount cashReceived,
             DenominationCount changeGiven,
             Money buyInAmount,
             std::string label,
             Money prize,
             Money bounty,
             Money rake)
    : Transaction(std::move(playerId)),
      cashReceived_(std::move(cashReceived)),
      changeGiven_(std::move(changeGiven)),
      buyInAmount_(buyInAmount),
      prize_(prize),
      bounty_(bounty),
      rake_(rake) {
    notes_ = std::move(label);
    if (prize_.isZero() && bounty_.isZero() && rake_.isZero()) {
        prize_ = buyInAmount_;
    }
}

void BuyIn::setSplit(Money prize, Money bounty, Money rake) {
    prize_ = prize;
    bounty_ = bounty;
    rake_ = rake;
}

void BuyIn::apply(Cashbox& cashbox) const {
    cashbox.applyCount(cashReceived_, +1, MovementReason::BuyIn, playerId_, "Buy-in cash in");
    cashbox.applyCount(changeGiven_, -1, MovementReason::BuyIn, playerId_, "Buy-in change");
}

void BuyIn::reverse(Cashbox& cashbox) const {
    cashbox.applyCount(changeGiven_, +1, MovementReason::Undo, playerId_, "Undo buy-in change");
    cashbox.applyCount(cashReceived_, -1, MovementReason::Undo, playerId_, "Undo buy-in cash in");
}

std::string BuyIn::describe() const {
    std::string text = isRebuy() ? "Rebuy " : "Buy-in ";
    text += buyInAmount_.toString();
    text += " (cash in " + cashReceived_.total().toString();
    text += ", change " + changeGiven_.total().toString() + ")";
    if (!notes_.empty()) {
        text += " - " + notes_;
    }
    return text;
}

} // namespace pokersoc::domain
