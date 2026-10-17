#pragma once

#include <memory>
#include <string>
#include <utility>

namespace pokersoc::domain {
class Cashbox;
class Transaction;
class TransactionLog;
} // namespace pokersoc::domain

namespace pokersoc::persistence {
class AppStore;
}

namespace pokersoc::application {

/// The common outcome of a money operation.
struct OperationResult {
    bool success = false;
    std::string message;
    const domain::Transaction* transaction = nullptr; // owned by the log

    static OperationResult fail(std::string why) {
        OperationResult result;
        result.message = std::move(why);
        return result;
    }
};

/// The single path by which money transactions enter (and leave) the session.
/// Every service hands its finished transaction here, so the crash-safe order —
/// check against the cashbox, persist, apply, log — lives in one place, and undo
/// is the exact mirror of it.
class Ledger {
public:
    Ledger(domain::Cashbox& cashbox, domain::TransactionLog& log, persistence::AppStore& store)
        : cashbox_(cashbox), log_(log), store_(store) {}

    /// Check the transaction can be applied (the cashbox holds every note/coin
    /// it would pay out), then persist, apply and log it. Throws
    /// std::logic_error, leaving everything untouched, if the cashbox is short.
    template <class T>
    const T& commit(std::unique_ptr<T> transaction) {
        const T& stored = *transaction;
        commitTransaction(std::move(transaction));
        return stored;
    }

    /// Reverse a recorded transaction out of the cashbox and the log, marking it
    /// void in the store. Fails if the cashbox no longer holds what was paid in.
    OperationResult undo(long long id);

    domain::Cashbox& cashbox() noexcept { return cashbox_; }
    const domain::Cashbox& cashbox() const noexcept { return cashbox_; }
    const domain::TransactionLog& log() const noexcept { return log_; }

private:
    void commitTransaction(std::unique_ptr<domain::Transaction> transaction);

    domain::Cashbox& cashbox_;
    domain::TransactionLog& log_;
    persistence::AppStore& store_;
};

} // namespace pokersoc::application
