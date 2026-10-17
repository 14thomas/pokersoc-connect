#include "domain/transaction/CashOut.h"

#include <utility>

#include "domain/cashbox/Cashbox.h"

namespace pokersoc::domain {

CashOut::CashOut(std::string playerId,
                 DenominationCount chipsReturned,
                 DenominationCount cashPaidOut,
                 Money tip,
                 DenominationCount extraCash,
                 std::vector<SaleLine> food)
    : Transaction(std::move(playerId)),
      chipsReturned_(std::move(chipsReturned)),
      cashPaidOut_(std::move(cashPaidOut)),
      tip_(tip),
      extraCash_(std::move(extraCash)),
      food_(std::move(food)) {}

void CashOut::apply(Cashbox& cashbox) const {
    cashbox.applyCount(extraCash_, +1, MovementReason::ExtraCash, playerId_, "Cash-out extra cash");
    cashbox.applyCount(cashPaidOut_, -1, MovementReason::CashOut, playerId_, "Cash-out payout");
}

void CashOut::reverse(Cashbox& cashbox) const {
    cashbox.applyCount(cashPaidOut_, +1, MovementReason::Undo, playerId_, "Undo cash-out");
    cashbox.applyCount(extraCash_, -1, MovementReason::Undo, playerId_, "Undo cash-out extra cash");
}

std::string CashOut::describe() const {
    std::string text = "Cash-out: chips " + chipValue().toString() + ", paid " + amount().toString();
    if (!food_.empty()) {
        text += ", food " + foodTotal().toString() + " (" + describeSaleLines(food_) + ")";
    }
    if (!tip_.isZero()) {
        text += ", tip " + tip_.toString();
    }
    if (!extraCash_.empty()) {
        text += ", extra cash " + extraCash_.total().toString();
    }
    return text;
}

} // namespace pokersoc::domain
