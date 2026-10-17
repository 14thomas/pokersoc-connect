#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "domain/food/Product.h"

namespace pokersoc::domain {

/// The products on sale, plus a handful of "quick slots" pinned to the front of
/// the food menu for the most common items.
class ProductCatalog {
public:
    static constexpr int kSlotCount = 6;

    /// Insert or replace by id (the id must already be assigned).
    const Product& upsert(Product product);
    /// Removes the product and clears any slot pointing at it.
    bool remove(long long id);
    void clear();

    const Product* find(long long id) const;
    /// Case-insensitive exact name match.
    const Product* findByName(const std::string& name) const;

    /// All products sorted by name.
    std::vector<const Product*> all() const;
    std::size_t size() const noexcept { return products_.size(); }
    bool empty() const noexcept { return products_.empty(); }

    /// Slot numbers are 1..kSlotCount; productId 0 empties the slot.
    void setSlot(int slot, long long productId);
    /// The product in a slot, or nullptr if empty / out of range.
    const Product* slot(int slot) const;
    long long slotProductId(int slot) const;

private:
    std::map<long long, Product> products_;
    std::array<long long, kSlotCount> slots_{};
};

} // namespace pokersoc::domain
