#pragma once

#include "domain/money/Denomination.h"

namespace pokersoc::domain {

/// Abstract base for gaming tokens (chips and plaques).
///
/// Chip counting rule (ported from the C# `ChipCounting`): 25c and $25 tokens
/// are counted in stacks of 4; every other token is counted in stacks of 5.
class ChipDenomination : public Denomination {
public:
    static constexpr int kDefaultStackSize = 5;
    static constexpr int kSpecialStackSize = 4;

    int stackSize() const noexcept;

protected:
    using Denomination::Denomination;
};

/// A round poker chip, e.g. 5c, 25c, $1, $5.
class Chip final : public ChipDenomination {
public:
    explicit Chip(std::int64_t cents) : ChipDenomination(cents) {}

    DenominationCategory category() const noexcept override;
    std::string label() const override;
    std::string assetName() const override;
};

/// A rectangular high-value plaque, e.g. $25, $100.
class Plaque final : public ChipDenomination {
public:
    explicit Plaque(std::int64_t cents) : ChipDenomination(cents) {}

    DenominationCategory category() const noexcept override;
    std::string label() const override;
    std::string assetName() const override;
};

} // namespace pokersoc::domain
