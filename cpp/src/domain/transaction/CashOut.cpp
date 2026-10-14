#include "domain/transaction/CashOut.h"

#include <utility>

#include "domain/cashbox/Cashbox.h"

namespace pokersoc::domain {

CashOut::CashOut(std::string playerId,
                 DenominationCount chipsReturned,
                 DenominationCount cashPaidOut,
                 Money tip)
    : Transaction(std::move(playerId)),
      chipsReturned_(std::move(chipsReturned)),
      cashPaidOut_(std::move(cashPaidOut)),
      tip_(tip) {}

void CashOut::apply(Cashbox& cashbox) const {
    cashbox.applyCount(cashPaidOut_, -1, MovementReason::CashOut, playerId_, "Cash-out payout");
}

void CashOut::reverse(Cashbox& cashbox) const {
    cashbox.applyCount(cashPaidOut_, +1, MovementReason::Undo, playerId_, "Undo cash-out");
}

std::string CashOut::describe() const {
    std::string text = "CASHOUT player=";
    text += playerId_.empty() ? "(none)" : playerId_;
    text += " chips=" + chipValue().toString();
    text += " paid=" + amount().toString();
    text += " tip=" + tip_.toString();
    return text;
}

} // namespace pokersoc::domain
