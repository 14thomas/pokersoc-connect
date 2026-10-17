#pragma once

#include <algorithm>
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

    const Transaction* find(long long id) const noexcept {
        for (const auto& transaction : entries_) {
            if (transaction->id() == id) {
                return transaction.get();
            }
        }
        return nullptr;
    }

    /// Take the transaction with this id out of the log (nullptr if absent).
    std::unique_ptr<Transaction> remove(long long id) {
        const auto it = std::find_if(entries_.begin(), entries_.end(),
                                     [id](const auto& entry) { return entry->id() == id; });
        if (it == entries_.end()) {
            return nullptr;
        }
        std::unique_ptr<Transaction> removed = std::move(*it);
        entries_.erase(it);
        return removed;
    }

    void clear() noexcept { entries_.clear(); }

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
