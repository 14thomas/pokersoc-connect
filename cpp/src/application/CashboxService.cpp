#include "application/CashboxService.h"

#include <memory>
#include <stdexcept>
#include <utility>

#include "domain/money/DenominationCount.h"
#include "domain/session/SessionMode.h"
#include "domain/transaction/CashboxTransactions.h"

namespace pokersoc::application {

using domain::DenominationCount;

namespace {

template <class T>
OperationResult commitTo(Ledger& ledger, std::unique_ptr<T> transaction) {
    OperationResult result;
    try {
        const T& stored = ledger.commit(std::move(transaction));
        result.success = true;
        result.transaction = &stored;
        result.message = stored.describe();
    } catch (const std::logic_error& error) {
        result.message = error.what();
    }
    return result;
}

} // namespace

OperationResult CashboxService::addFloat(const DenominationCount& cash) {
    if (cash.empty()) {
        return OperationResult::fail("Nothing to add.");
    }
    return commitTo(ledger_, std::make_unique<domain::FloatAdd>(cash));
}

OperationResult CashboxService::addTip(const DenominationCount& cash,
                                       const std::string& playerId,
                                       const std::string& note) {
    if (cash.empty()) {
        return OperationResult::fail("No tip entered.");
    }
    auto tip = std::make_unique<domain::CashTip>(cash, playerId);
    tip->setNotes(note);
    return commitTo(ledger_, std::move(tip));
}

OperationResult CashboxService::recordLostChips(const DenominationCount& chips,
                                                const std::string& note) {
    if (!mode_.chipsHaveCashValue()) {
        return OperationResult::fail("Lost chips are only banked in cash games.");
    }
    if (chips.empty()) {
        return OperationResult::fail("No chips entered.");
    }
    auto lost = std::make_unique<domain::LostChips>(chips);
    lost->setNotes(note);
    return commitTo(ledger_, std::move(lost));
}

OperationResult CashboxService::exchange(const DenominationCount& given,
                                         const DenominationCount& received) {
    if (given.empty() || received.empty()) {
        return OperationResult::fail("Both sides of the exchange need an amount.");
    }
    if (given.total() != received.total()) {
        return OperationResult::fail("Exchange does not balance: giving " +
                                     given.total().toString() + " for " +
                                     received.total().toString() + ".");
    }
    return commitTo(ledger_, std::make_unique<domain::Exchange>(given, received));
}

} // namespace pokersoc::application
