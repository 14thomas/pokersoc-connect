#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "domain/cashbox/CashboxMovement.h"
#include "domain/cashbox/MovementReason.h"
#include "domain/money/Denomination.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"

namespace pokersoc::domain {

class DenominationTable;

class Cashbox {
public:
    using Ptr = std::shared_ptr<const Denomination>;

    /// Build an empty cashbox over the given currency denominations.
    explicit Cashbox(std::vector<Ptr> currencyDenominations);

    /// Convenience: an empty cashbox using a table's currency set.
    static Cashbox withStandardCurrency(const DenominationTable& table);

    // ---- mutations (each records a movement) ----
    void deposit(const Ptr& denomination, int qty, MovementReason reason,
                 std::string playerId = "", std::string note = "");
    /// Throws std::logic_error if there is not enough stock to remove.
    void withdraw(const Ptr& denomination, int qty, MovementReason reason,
                  std::string playerId = "", std::string note = "");

    /// Deposit a whole tally as the opening / additional float.
    void applyFloat(const DenominationCount& amount);
    /// Deposit (sign = +1) or withdraw (sign = -1) a whole tally.
    void applyCount(const DenominationCount& amount, int sign, MovementReason reason,
                    const std::string& playerId = "", const std::string& note = "");

    // ---- queries ----
    int quantityOf(std::int64_t cents) const noexcept;
    Money total() const noexcept { return stock_.total(); }
    const DenominationCount& stock() const noexcept { return stock_; }
    const std::vector<CashboxMovement>& movements() const noexcept { return movements_; }
    const std::vector<Ptr>& currency() const noexcept { return currency_; }

    /// Greedy change for `amount` limited by current stock, high denominations
    /// first. Returns nullopt if exact change cannot be made from what we hold.
    std::optional<DenominationCount> makeChange(Money amount) const;

private:
    Ptr denominationForCents(std::int64_t cents) const noexcept;

    std::vector<Ptr> currency_; // sorted high-to-low
    DenominationCount stock_;
    std::vector<CashboxMovement> movements_;
};

} // namespace pokersoc::domain
