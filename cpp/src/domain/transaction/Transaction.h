#pragma once

#include <string>
#include <utility>

#include "domain/money/Money.h"

namespace pokersoc::domain {

class Cashbox;

enum class TransactionType { BuyIn, Rebuy, CashOut };

inline const char* transactionTypeName(TransactionType type) noexcept {
    switch (type) {
        case TransactionType::BuyIn:   return "BUYIN";
        case TransactionType::Rebuy:   return "REBUY";
        case TransactionType::CashOut: return "CASHOUT";
    }
    return "?";
}

/// Abstract base for anything that moves money for a player. Subclasses decide
/// how they apply to (and reverse out of) the cashbox, which keeps the undo
/// story uniform. BuyIn / Rebuy will arrive in a later pass; CashOut is first.
class Transaction {
public:
    virtual ~Transaction() = default;

    virtual TransactionType type() const noexcept = 0;
    /// Net cash leaving the house (positive) for this transaction.
    virtual Money amount() const noexcept = 0;

    virtual void apply(Cashbox& cashbox) const = 0;
    virtual void reverse(Cashbox& cashbox) const = 0;

    virtual std::string describe() const = 0;

    const std::string& playerId() const noexcept { return playerId_; }

protected:
    explicit Transaction(std::string playerId) : playerId_(std::move(playerId)) {}

    std::string playerId_;
};

} // namespace pokersoc::domain
