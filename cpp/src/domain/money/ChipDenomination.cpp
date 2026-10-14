#include "domain/money/ChipDenomination.h"

namespace pokersoc::domain {

int ChipDenomination::stackSize() const noexcept {
    const bool stacksOfFour = (cents_ == 25 || cents_ == 2500);
    return stacksOfFour ? kSpecialStackSize : kDefaultStackSize;
}

DenominationCategory Chip::category() const noexcept { return DenominationCategory::Chip; }
std::string Chip::label() const { return formatLabel(cents_); }
std::string Chip::assetName() const { return "chip_" + formatAssetToken(cents_) + ".png"; }

DenominationCategory Plaque::category() const noexcept { return DenominationCategory::Plaque; }
std::string Plaque::label() const { return formatLabel(cents_); }
std::string Plaque::assetName() const { return "plaque_" + formatAssetToken(cents_) + ".png"; }

} // namespace pokersoc::domain
