#include "domain/money/DenominationTable.h"

#include <algorithm>

#include "domain/money/CashDenomination.h"
#include "domain/money/ChipDenomination.h"

namespace pokersoc::domain {

DenominationTable DenominationTable::standardAustralian() {
    DenominationTable table;

    // Notes: $100, $50, $20, $10, $5.
    for (std::int64_t cents : {10000, 5000, 2000, 1000, 500}) {
        table.addCurrency(std::make_shared<Note>(cents));
    }
    // Coins: $2, $1, 50c, 20c, 10c, 5c.
    for (std::int64_t cents : {200, 100, 50, 20, 10, 5}) {
        table.addCurrency(std::make_shared<Coin>(cents));
    }

    // Chips: $5, $1, 25c, 5c.
    for (std::int64_t cents : {500, 100, 25, 5}) {
        table.addChip(std::make_shared<Chip>(cents));
    }
    // Plaques: $100, $25.
    for (std::int64_t cents : {10000, 2500}) {
        table.addChip(std::make_shared<Plaque>(cents));
    }

    return table;
}

std::vector<DenominationTable::Ptr> DenominationTable::all() const {
    std::vector<Ptr> combined;
    combined.reserve(currency_.size() + chips_.size());
    combined.insert(combined.end(), currency_.begin(), currency_.end());
    combined.insert(combined.end(), chips_.begin(), chips_.end());
    return combined;
}

DenominationTable::Ptr DenominationTable::findCurrency(std::int64_t cents) const {
    return findByCents(currency_, cents);
}

DenominationTable::Ptr DenominationTable::findChip(std::int64_t cents) const {
    return findByCents(chips_, cents);
}

void DenominationTable::addCurrency(Ptr denomination) {
    insertSortedDesc(currency_, std::move(denomination));
}

void DenominationTable::addChip(Ptr denomination) {
    insertSortedDesc(chips_, std::move(denomination));
}

void DenominationTable::insertSortedDesc(std::vector<Ptr>& list, Ptr denomination) {
    if (!denomination) {
        return;
    }
    const auto position = std::lower_bound(
        list.begin(), list.end(), denomination->cents(),
        [](const Ptr& existing, std::int64_t cents) { return existing->cents() > cents; });
    list.insert(position, std::move(denomination));
}

DenominationTable::Ptr DenominationTable::findByCents(const std::vector<Ptr>& list,
                                                      std::int64_t cents) {
    for (const auto& denomination : list) {
        if (denomination->cents() == cents) {
            return denomination;
        }
    }
    return nullptr;
}

} // namespace pokersoc::domain
