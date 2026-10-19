#include "domain/cashbox/Cashbox.h"

#include <algorithm>
#include <limits>
#include <numeric>
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
    if (!allowOverdraw_ && stock_.quantityOf(denomination) < qty) {
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
        return makeChangeExact(amount);
    }
    return change;
}

bool Cashbox::canSupply(const DenominationCount& count) const noexcept {
    for (const auto& entry : count.entries()) {
        if (entry.qty > 0 && stock_.quantityOf(entry.denomination) < entry.qty) {
            return false;
        }
    }
    return true;
}

// Bounded knapsack minimising piece count. Each denomination's stock is split
// into power-of-two bundles so the search is a 0/1 knapsack over a few dozen
// items; amounts are scaled down by the gcd of the face values (5c in AUD).
std::optional<DenominationCount> Cashbox::makeChangeExact(Money amount) const {
    constexpr std::int64_t kMaxUnits = 100000;

    struct Bundle {
        Ptr denomination;
        int qty;
        std::int64_t units;
    };

    std::int64_t unit = 0;
    for (const auto& denomination : currency_) {
        if (stock_.quantityOf(denomination) > 0 && denomination->cents() > 0) {
            unit = std::gcd(unit, denomination->cents());
        }
    }
    if (unit == 0 || amount.cents() % unit != 0) {
        return std::nullopt;
    }
    const std::int64_t target = amount.cents() / unit;
    if (target > kMaxUnits) {
        return std::nullopt;
    }

    std::vector<Bundle> bundles;
    for (const auto& denomination : currency_) {
        int remaining = stock_.quantityOf(denomination);
        if (remaining <= 0 || denomination->cents() <= 0) {
            continue;
        }
        const std::int64_t faceUnits = denomination->cents() / unit;
        for (int size = 1; remaining > 0; size *= 2) {
            const int take = std::min(size, remaining);
            bundles.push_back(Bundle{denomination, take, faceUnits * take});
            remaining -= take;
        }
    }

    const int kInf = std::numeric_limits<int>::max() / 2;
    const auto width = static_cast<std::size_t>(target) + 1;
    std::vector<int> best(width, kInf);
    std::vector<std::vector<char>> used(bundles.size(), std::vector<char>(width, 0));
    best[0] = 0;

    for (std::size_t i = 0; i < bundles.size(); ++i) {
        const std::int64_t units = bundles[i].units;
        for (std::int64_t t = target; t >= units; --t) {
            const int candidate = best[static_cast<std::size_t>(t - units)] + bundles[i].qty;
            if (candidate < best[static_cast<std::size_t>(t)]) {
                best[static_cast<std::size_t>(t)] = candidate;
                used[i][static_cast<std::size_t>(t)] = 1;
            }
        }
    }
    if (best[static_cast<std::size_t>(target)] >= kInf) {
        return std::nullopt;
    }

    DenominationCount change;
    std::int64_t t = target;
    for (std::size_t i = bundles.size(); i-- > 0;) {
        if (used[i][static_cast<std::size_t>(t)]) {
            change.add(bundles[i].denomination, bundles[i].qty);
            t -= bundles[i].units;
        }
    }
    return change;
}

} // namespace pokersoc::domain
