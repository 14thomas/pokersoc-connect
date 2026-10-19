#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "domain/money/Denomination.h"
#include "domain/money/Money.h"

namespace pokersoc::domain {

/// A tally of denominations to quantities (a count of notes/coins/chips).
///
/// Reused everywhere money is counted by denomination: an opening float, the
/// chips a player hands back, the cash paid out, change, etc. Entries are kept
/// sorted high-to-low and zero quantities are dropped automatically.
class DenominationCount {
public:
    using Ptr = std::shared_ptr<const Denomination>;

    struct Entry {
        Ptr denomination;
        int qty = 0;
    };

    /// Add (or, with a negative qty, remove) a number of a denomination.
    void add(const Ptr& denomination, int qty);

    int quantityOf(const Ptr& denomination) const noexcept;
    Money total() const noexcept;
    bool empty() const noexcept { return entries_.empty(); }
    std::size_t size() const noexcept { return entries_.size(); }

    const std::vector<Entry>& entries() const noexcept { return entries_; }

private:
    std::vector<Entry> entries_; // sorted by cents descending, all qty != 0
};

} // namespace pokersoc::domain
