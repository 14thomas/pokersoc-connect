#include "domain/money/CashDenomination.h"

namespace pokersoc::domain {

DenominationCategory Note::category() const noexcept { return DenominationCategory::Note; }
std::string Note::label() const { return formatLabel(cents_); }
std::string Note::assetName() const { return "note_" + formatAssetToken(cents_) + ".png"; }

DenominationCategory Coin::category() const noexcept { return DenominationCategory::Coin; }
std::string Coin::label() const { return formatLabel(cents_); }
std::string Coin::assetName() const { return "coin_" + formatAssetToken(cents_) + ".png"; }

} // namespace pokersoc::domain
