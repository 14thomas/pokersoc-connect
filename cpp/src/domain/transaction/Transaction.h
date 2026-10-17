#pragma once

#include <string>
#include <utility>

#include "domain/money/Money.h"

namespace pokersoc::domain {

class Cashbox;

/// Every kind of money event the session records. Each one is a Transaction
/// subclass, so the activity log, persistence and undo treat them uniformly.
enum class TransactionType {
    BuyIn,
    Rebuy,
    CashOut,
    FloatAdd,
    CashTip,
    LostChips,
    Exchange,
    FoodSale,
    PrizePayout,
    BountyPayout
};

inline const char* transactionTypeName(TransactionType type) noexcept {
    switch (type) {
        case TransactionType::BuyIn:        return "BUYIN";
        case TransactionType::Rebuy:        return "REBUY";
        case TransactionType::CashOut:      return "CASHOUT";
        case TransactionType::FloatAdd:     return "FLOAT_ADD";
        case TransactionType::CashTip:      return "TIP";
        case TransactionType::LostChips:    return "LOST_CHIPS";
        case TransactionType::Exchange:     return "EXCHANGE";
        case TransactionType::FoodSale:     return "FOOD_SALE";
        case TransactionType::PrizePayout:  return "PRIZE";
        case TransactionType::BountyPayout: return "BOUNTY";
    }
    return "?";
}

/// Abstract base for anything that moves money. Subclasses decide how they
/// apply to (and reverse out of) the cashbox, which keeps undo uniform.
///
/// `id` and `time` are assigned by the store when the transaction is persisted
/// (or replayed); a transaction that has not been saved yet has id 0.
class Transaction {
public:
    virtual ~Transaction() = default;

    virtual TransactionType type() const noexcept = 0;
    /// The headline figure shown in the activity log (buy-in amount, cash paid
    /// out, float added, ...).
    virtual Money amount() const noexcept = 0;

    virtual void apply(Cashbox& cashbox) const = 0;
    virtual void reverse(Cashbox& cashbox) const = 0;

    virtual std::string describe() const = 0;

    const std::string& playerId() const noexcept { return playerId_; }
    long long id() const noexcept { return id_; }
    const std::string& time() const noexcept { return time_; } // UTC "YYYY-MM-DD HH:MM:SS"
    const std::string& notes() const noexcept { return notes_; }

    void setId(long long id) noexcept { id_ = id; }
    void setTime(std::string time) { time_ = std::move(time); }
    void setNotes(std::string notes) { notes_ = std::move(notes); }

protected:
    explicit Transaction(std::string playerId) : playerId_(std::move(playerId)) {}

    std::string playerId_;
    long long id_ = 0;
    std::string time_;
    std::string notes_;
};

} // namespace pokersoc::domain
