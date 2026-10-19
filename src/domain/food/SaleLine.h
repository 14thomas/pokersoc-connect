#pragma once

#include <string>
#include <vector>

#include "domain/money/Money.h"

namespace pokersoc::domain {

/// One line of a food purchase. The name and price are copied from the catalog
/// at the time of sale, so history stays correct if the catalog changes later.
struct SaleLine {
    std::string productName;
    Money unitPrice;
    int qty = 0;

    Money total() const noexcept { return unitPrice * qty; }
};

inline Money saleLinesTotal(const std::vector<SaleLine>& lines) noexcept {
    Money sum;
    for (const auto& line : lines) {
        sum += line.total();
    }
    return sum;
}

/// "Pie x2, Coke" style summary for logs and receipts.
inline std::string describeSaleLines(const std::vector<SaleLine>& lines) {
    std::string text;
    for (const auto& line : lines) {
        if (!text.empty()) text += ", ";
        text += line.productName;
        if (line.qty > 1) text += " x" + std::to_string(line.qty);
    }
    return text;
}

} // namespace pokersoc::domain
