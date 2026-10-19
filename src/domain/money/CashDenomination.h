#pragma once

#include "domain/money/Denomination.h"

namespace pokersoc::domain {

/// Abstract base for real currency (physical money the house holds as float).
class CashDenomination : public Denomination {
protected:
    using Denomination::Denomination;
};

/// A banknote, e.g. $5 .. $100.
class Note final : public CashDenomination {
public:
    explicit Note(std::int64_t cents) : CashDenomination(cents) {}

    DenominationCategory category() const noexcept override;
    std::string label() const override;
    std::string assetName() const override;
};

/// A coin, e.g. 5c .. $2.
class Coin final : public CashDenomination {
public:
    explicit Coin(std::int64_t cents) : CashDenomination(cents) {}

    DenominationCategory category() const noexcept override;
    std::string label() const override;
    std::string assetName() const override;
};

} // namespace pokersoc::domain
