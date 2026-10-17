#pragma once

#include <memory>
#include <vector>

#include "domain/money/Money.h"
#include "domain/transaction/Transaction.h"

namespace pokersoc::domain {

/// Ordered history of transactions for the current session. Owns the
/// transactions; rebuilt from the database on startup.
class TransactionLog {
public:
    void add(std::unique_ptr<Transaction> transaction) {
        entries_.push_back(std::move(transaction));
    }

    const std::vector<std::unique_ptr<Transaction>>& entries() const noexcept {
        return entries_;
    }

    std::size_t size() const noexcept { return entries_.size(); }
    bool empty() const noexcept { return entries_.empty(); }

    /// Total cash paid out across all cash-out transactions.
    Money totalPaidOut() const noexcept {
        Money sum;
        for (const auto& transaction : entries_) {
            if (transaction->type() == TransactionType::CashOut) {
                sum += transaction->amount();
            }
        }
        return sum;
    }

private:
    std::vector<std::unique_ptr<Transaction>> entries_;
};

} // namespace pokersoc::domain
