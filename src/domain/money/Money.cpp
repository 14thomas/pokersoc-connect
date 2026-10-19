#include "domain/money/Money.h"

#include <cstdio>

namespace pokersoc::domain {

Money Money::fromDollars(double dollars) noexcept {
    const double scaled = dollars * 100.0;
    // Round half away from zero so e.g. 0.005 -> 1 cent, -0.005 -> -1 cent.
    const std::int64_t rounded =
        static_cast<std::int64_t>(scaled < 0.0 ? scaled - 0.5 : scaled + 0.5);
    return Money(rounded);
}

std::string Money::toString() const {
    std::int64_t magnitude = cents_ < 0 ? -cents_ : cents_;
    const long long dollars = static_cast<long long>(magnitude / 100);
    const int remainder = static_cast<int>(magnitude % 100);

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%s$%lld.%02d",
                  cents_ < 0 ? "-" : "", dollars, remainder);
    return std::string(buffer);
}

} // namespace pokersoc::domain
