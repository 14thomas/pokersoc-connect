#include "application/Ledger.h"

#include <stdexcept>

#include "domain/cashbox/Cashbox.h"
#include "domain/transaction/Transaction.h"
#include "domain/transaction/TransactionLog.h"
#include "persistence/AppStore.h"

namespace pokersoc::application {

void Ledger::commitTransaction(std::unique_ptr<domain::Transaction> transaction) {
    domain::Cashbox probe = cashbox_;
    transaction->apply(probe);              // throws if the cashbox is short

    store_.recordTransaction(*transaction); // persist first (crash-safe); assigns id/time
    transaction->apply(cashbox_);           // then mutate the in-memory cashbox
    log_.add(std::move(transaction));
}

OperationResult Ledger::undo(long long id) {
    const domain::Transaction* existing = log_.find(id);
    if (!existing) {
        return OperationResult::fail("No transaction #" + std::to_string(id) + " in this session.");
    }

    domain::Cashbox probe = cashbox_;
    try {
        existing->reverse(probe);
    } catch (const std::logic_error&) {
        return OperationResult::fail(
            "Cannot undo #" + std::to_string(id) +
            ": the cashbox no longer holds the notes/coins it took in.");
    }

    const std::string description = existing->describe();
    store_.voidTransaction(id);
    std::unique_ptr<domain::Transaction> removed = log_.remove(id);
    removed->reverse(cashbox_);

    OperationResult result;
    result.success = true;
    result.message = "Undid #" + std::to_string(id) + ": " + description;
    return result;
}

} // namespace pokersoc::application
