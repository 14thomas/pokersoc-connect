#include "domain/money/DenominationCount.h"

#include <algorithm>

namespace pokersoc::domain {

void DenominationCount::add(const Ptr& denomination, int qty) {
    if (!denomination || qty == 0) {
        return;
    }

    const auto existing = std::find_if(
        entries_.begin(), entries_.end(),
        [&](const Entry& entry) { return entry.denomination == denomination; });

    if (existing != entries_.end()) {
        existing->qty += qty;
        if (existing->qty == 0) {
            entries_.erase(existing);
        }
        return;
    }

    const auto position = std::lower_bound(
        entries_.begin(), entries_.end(), denomination->cents(),
        [](const Entry& entry, std::int64_t cents) {
            return entry.denomination->cents() > cents;
        });
    entries_.insert(position, Entry{denomination, qty});
}

int DenominationCount::quantityOf(const Ptr& denomination) const noexcept {
    for (const auto& entry : entries_) {
        if (entry.denomination == denomination) {
            return entry.qty;
        }
    }
    return 0;
}

Money DenominationCount::total() const noexcept {
    std::int64_t cents = 0;
    for (const auto& entry : entries_) {
        cents += entry.denomination->cents() * entry.qty;
    }
    return Money::fromCents(cents);
}

} // namespace pokersoc::domain
