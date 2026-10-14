#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "domain/money/Denomination.h"

namespace pokersoc::domain {

/// The set of denominations in play, split into currency (notes + coins) and
/// chips (chips + plaques). Each list is kept sorted high-to-low so it can be
/// used directly for change-making and for rendering count grids.
class DenominationTable {
public:
    using Ptr = std::shared_ptr<const Denomination>;

    /// The standard Australian currency + the club's chip set.
    static DenominationTable standardAustralian();

    const std::vector<Ptr>& currency() const noexcept { return currency_; }
    const std::vector<Ptr>& chips() const noexcept { return chips_; }

    /// Currency followed by chips, in one list.
    std::vector<Ptr> all() const;

    /// Look up by face value within a group (nullptr if absent).
    Ptr findCurrency(std::int64_t cents) const;
    Ptr findChip(std::int64_t cents) const;

    void addCurrency(Ptr denomination);
    void addChip(Ptr denomination);

private:
    static void insertSortedDesc(std::vector<Ptr>& list, Ptr denomination);
    static Ptr findByCents(const std::vector<Ptr>& list, std::int64_t cents);

    std::vector<Ptr> currency_;
    std::vector<Ptr> chips_;
};

} // namespace pokersoc::domain
