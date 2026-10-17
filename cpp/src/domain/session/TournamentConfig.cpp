#include "domain/session/TournamentConfig.h"

#include "domain/player/Player.h"

namespace pokersoc::domain {

namespace {

Money cents(std::int64_t value) noexcept { return Money::fromCents(value); }

/// Nicely cased membership label for receipts / the UI ("Gold", not "GOLD").
const char* membershipLabel(MembershipType membership) noexcept {
    switch (membership) {
        case MembershipType::Bronze:   return "Bronze";
        case MembershipType::Silver:   return "Silver";
        case MembershipType::Gold:     return "Gold";
        case MembershipType::Platinum: return "Platinum";
        case MembershipType::Diamond:  return "Diamond";
        case MembershipType::None:     return "Member";
    }
    return "Member";
}

} // namespace

const char* tournamentPricingName(TournamentPricing pricing) noexcept {
    return pricing == TournamentPricing::MembershipTiers ? "MEMBERSHIP_TIERS"
                                                         : "ARC_VS_NON_ARC";
}

TournamentPricing tournamentPricingFromName(std::string_view name) noexcept {
    if (name == "MEMBERSHIP_TIERS") return TournamentPricing::MembershipTiers;
    return TournamentPricing::ArcVsNonArc;
}

Money TournamentConfig::priceFor(const Player& player, bool isRebuy) const {
    if (usesMembershipPricing()) {
        switch (player.membership()) {
            case MembershipType::Silver:   return cents(isRebuy ? silverRebuyCents : silverBuyInCents);
            case MembershipType::Gold:     return cents(isRebuy ? goldRebuyCents : goldBuyInCents);
            case MembershipType::Platinum: return cents(isRebuy ? platinumRebuyCents : platinumBuyInCents);
            case MembershipType::Diamond:  return cents(isRebuy ? diamondRebuyCents : diamondBuyInCents);
            case MembershipType::Bronze:
            case MembershipType::None:
                return cents(isRebuy ? bronzeRebuyCents : bronzeBuyInCents);
        }
        return cents(isRebuy ? bronzeRebuyCents : bronzeBuyInCents);
    }

    const bool isArc = player.arcStatus() == ArcStatus::Arc;
    if (isArc) return cents(isRebuy ? arcRebuyCents : arcBuyInCents);
    return cents(isRebuy ? nonArcRebuyCents : nonArcBuyInCents);
}

std::string TournamentConfig::labelFor(const Player& player, bool isRebuy) const {
    const char* suffix = isRebuy ? " Rebuy" : " Buy-in";
    if (usesMembershipPricing()) {
        return std::string(membershipLabel(player.membership())) + suffix;
    }
    const bool isArc = player.arcStatus() == ArcStatus::Arc;
    return std::string(isArc ? "ARC Member" : "Non-ARC Member") + suffix;
}

bool TournamentConfig::eligible(const Player& player) const {
    if (arcOnly && player.arcStatus() != ArcStatus::Arc) {
        return false;
    }
    return true;
}

std::string TournamentConfig::pricingSummary() const {
    if (usesMembershipPricing()) {
        return "Membership tiers (Bronze-Diamond)";
    }
    return arcOnly ? "ARC vs Non-ARC (ARC only)" : "ARC vs Non-ARC";
}

} // namespace pokersoc::domain
