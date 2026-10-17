#include "application/CashOutService.h"

#include <memory>
#include <utility>

#include "domain/cashbox/Cashbox.h"
#include "domain/transaction/CashOut.h"
#include "domain/transaction/TransactionLog.h"
#include "persistence/AppStore.h"

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

    auto transaction = std::make_unique<CashOut>(playerId, chipsReturned, *change, tip);

    // Persist first so the database is authoritative even if we crash next.
    store_.recordCashOut(*transaction);
    transaction->apply(cashbox_); // then mutate the in-memory cashbox

    result.success = true;
    result.message = "Paid " + owed.toString() + " to " +
                     (playerId.empty() ? "player" : playerId) + ".";
    result.change = std::move(*change);
    result.transaction = transaction.get();
    log_.add(std::move(transaction));
    return result;
}

} // namespace pokersoc::application
