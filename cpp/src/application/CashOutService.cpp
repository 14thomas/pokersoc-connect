#include "application/CashOutService.h"

#include "domain/cashbox/Cashbox.h"

namespace pokersoc::application {

using domain::CashOut;
using domain::DenominationCount;
using domain::Money;

CashOutService::Result CashOutService::cashOut(const std::string& playerId,
                                               const DenominationCount& chipsReturned,
                                               Money tip) {
    Result result;

    const Money chipValue = chipsReturned.total();
    if (chipValue.isZero()) {
        result.message = "No chips to cash out.";
        return result;
    }
    if (tip.isNegative() || tip > chipValue) {
        result.message = "Tip must be between zero and the chip value.";
        return result;
    }

    const Money owed = chipValue - tip;

    auto change = cashbox_.makeChange(owed);
    if (!change) {
        result.message = "Cannot make exact change for " + owed.toString() +
                         " from the current cashbox.";
        return result;
    }

    auto transaction =
        std::make_unique<CashOut>(playerId, chipsReturned, *change, tip);
    transaction->apply(cashbox_); // withdraws the change from the cashbox

    result.success = true;
    result.message = "Paid " + owed.toString() + " to " +
                     (playerId.empty() ? "player" : playerId) + ".";
    result.change = std::move(*change);
    result.transaction = std::move(transaction);
    return result;
}

} // namespace pokersoc::application
