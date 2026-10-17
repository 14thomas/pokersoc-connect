#include "domain/transaction/FoodSale.h"

#include <utility>

#include "domain/cashbox/Cashbox.h"

namespace pokersoc::domain {

FoodSale::FoodSale(std::string playerId,
                   std::vector<SaleLine> lines,
                   PaymentMethod method,
                   DenominationCount payment,
                   DenominationCount change)
    : Transaction(std::move(playerId)),
      lines_(std::move(lines)),
      method_(method),
      payment_(std::move(payment)),
      change_(std::move(change)) {}

void FoodSale::apply(Cashbox& cashbox) const {
    if (method_ != PaymentMethod::Cash) {
        return;
    }
    cashbox.applyCount(payment_, +1, MovementReason::Sale, playerId_, "Food sale");
    cashbox.applyCount(change_, -1, MovementReason::Sale, playerId_, "Food sale change");
}

void FoodSale::reverse(Cashbox& cashbox) const {
    if (method_ != PaymentMethod::Cash) {
        return;
    }
    cashbox.applyCount(change_, +1, MovementReason::Undo, playerId_, "Undo food sale change");
    cashbox.applyCount(payment_, -1, MovementReason::Undo, playerId_, "Undo food sale");
}

std::string FoodSale::describe() const {
    std::string text = "Food sale " + amount().toString() + ": " + describeSaleLines(lines_);
    text += method_ == PaymentMethod::Cash ? " (paid cash " : " (paid chips ";
    text += payment_.total().toString();
    if (!change_.empty()) {
        text += ", change " + change_.total().toString();
    }
    text += ")";
    return text;
}

} // namespace pokersoc::domain
