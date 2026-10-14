#include "domain/money/Denomination.h"

#include <cstdio>

namespace pokersoc::domain {

DenominationGroup Denomination::group() const noexcept {
    switch (category()) {
        case DenominationCategory::Note:
        case DenominationCategory::Coin:
            return DenominationGroup::Currency;
        case DenominationCategory::Chip:
        case DenominationCategory::Plaque:
            return DenominationGroup::Chip;
    }
    return DenominationGroup::Currency;
}

std::string Denomination::formatLabel(std::int64_t cents) {
    if (cents < 100) {
        return std::to_string(cents) + "c";
    }
    if (cents % 100 == 0) {
        return "$" + std::to_string(cents / 100);
    }
    const long long dollars = static_cast<long long>(cents / 100);
    const int remainder = static_cast<int>(cents % 100);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "$%lld.%02d", dollars, remainder);
    return std::string(buffer);
}

std::string Denomination::formatAssetToken(std::int64_t cents) {
    if (cents < 100) {
        return std::to_string(cents) + "c";
    }
    if (cents % 100 == 0) {
        return std::to_string(cents / 100);
    }
    // No current asset uses fractional dollars, but keep it unambiguous.
    const long long dollars = static_cast<long long>(cents / 100);
    const int remainder = static_cast<int>(cents % 100);
    return std::to_string(dollars) + "_" + std::to_string(remainder);
}

} // namespace pokersoc::domain
