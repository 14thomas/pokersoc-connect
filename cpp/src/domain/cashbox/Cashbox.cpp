#include "domain/cashbox/Cashbox.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "domain/money/DenominationTable.h"

namespace pokersoc::domain {

Cashbox::Cashbox(std::vector<Ptr> currencyDenominations)
    : currency_(std::move(currencyDenominations)) {
    // Keep denominations high-to-low so change-making is greedy-correct.
    std::sort(currency_.begin(), currency_.end(),
              [](const Ptr& a, const Ptr& b) { return a->cents() > b->cents(); });
}

Cashbox Cashbox::withStandardCurrency(const DenominationTable& table) {
    return Cashbox(table.currency());
}

Cashbox::Ptr Cashbox::denominationForCents(std::int64_t cents) const noexcept {
    for (const auto& denomination : currency_) {
        if (denomination->cents() == cents) {
            return denomination;
        }
    }
    return nullptr;
}

void Cashbox::deposit(const Ptr& denomination, int qty, MovementReason reason,
                      std::string playerId, std::string note) {
    if (!denomination || qty <= 0) {
        return;
    }
    stock_.add(denomination, qty);
    movements_.push_back(
        CashboxMovement{denomination, qty, reason, std::move(playerId), std::move(note)});
}

void Cashbox::withdraw(const Ptr& denomination, int qty, MovementReason reason,
                       std::string playerId, std::string note) {
    if (!denomination || qty <= 0) {
        return;
    }
    if (stock_.quantityOf(denomination) < qty) {
        throw std::logic_error("Cashbox: insufficient stock of " + denomination->label());
    }
    stock_.add(denomination, -qty);
    movements_.push_back(
        CashboxMovement{denomination, -qty, reason, std::move(playerId), std::move(note)});
}

void Cashbox::applyFloat(const DenominationCount& amount) {
    applyCount(amount, +1, MovementReason::FloatAdd, "", "Float");
}

void Cashbox::applyCount(const DenominationCount& amount, int sign, MovementReason reason,
                         const std::string& playerId, const std::string& note) {
    for (const auto& entry : amount.entries()) {
        if (sign >= 0) {
            deposit(entry.denomination, entry.qty, reason, playerId, note);
        } else {
            withdraw(entry.denomination, entry.qty, reason, playerId, note);
        }
    }
}

int Cashbox::quantityOf(std::int64_t cents) const noexcept {
    return stock_.quantityOf(denominationForCents(cents));
}

std::optional<DenominationCount> Cashbox::makeChange(Money amount) const {
    if (amount.isNegative()) {
        return std::nullopt;
    }

    DenominationCount change;
    std::int64_t remaining = amount.cents();

    for (const auto& denomination : currency_) { // high-to-low
        if (remaining <= 0) {
            break;
        }
        const std::int64_t face = denomination->cents();
        const int available = stock_.quantityOf(denomination);
        if (face <= 0 || available <= 0) {
            continue;
        }
        const int needed = static_cast<int>(remaining / face);
        const int take = std::min(needed, available);
        if (take > 0) {
            change.add(denomination, take);
            remaining -= static_cast<std::int64_t>(take) * face;
        }
    }

    if (remaining != 0) {
        return std::nullopt;
    }
    return change;
}

} // namespace pokersoc::domain
