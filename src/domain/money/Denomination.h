#pragma once

#include <cstdint>
#include <string>

#include "domain/money/Money.h"

namespace pokersoc::domain {

/// The concrete kind of a denomination.
enum class DenominationCategory { Note, Coin, Chip, Plaque };

/// The broad family a denomination belongs to.
enum class DenominationGroup { Currency, Chip };

/// Abstract base for a single physical face value (a note, coin, chip or
/// plaque). Knows its value in cents and how to present itself; subclasses
/// decide their category and asset naming.
class Denomination {
public:
    virtual ~Denomination() = default;

    std::int64_t cents() const noexcept { return cents_; }
    Money value() const noexcept { return Money::fromCents(cents_); }

    virtual DenominationCategory category() const noexcept = 0;
    /// Short display label, e.g. "$5", "50c".
    virtual std::string label() const = 0;
    /// Image file name used by the (future) GUI, e.g. "note_5.png".
    virtual std::string assetName() const = 0;

    DenominationGroup group() const noexcept;
    bool isCurrency() const noexcept { return group() == DenominationGroup::Currency; }
    bool isChip() const noexcept { return group() == DenominationGroup::Chip; }

protected:
    explicit Denomination(std::int64_t cents) noexcept : cents_(cents) {}

    /// "5c" for sub-dollar, "$5" / "$12.50" otherwise.
    static std::string formatLabel(std::int64_t cents);
    /// Asset filename token: "5c", "50c", "5", "100".
    static std::string formatAssetToken(std::int64_t cents);

    std::int64_t cents_;
};

} // namespace pokersoc::domain
