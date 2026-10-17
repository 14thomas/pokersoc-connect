#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "domain/money/Money.h"

namespace pokersoc::domain {

class Player;

/// How a tournament prices its buy-ins (ported from the original app's
/// TournamentSettings.PricingStructure).
enum class TournamentPricing { ArcVsNonArc, MembershipTiers };

const char* tournamentPricingName(TournamentPricing pricing) noexcept;
TournamentPricing tournamentPricingFromName(std::string_view name) noexcept;

/// The tunable settings of a tournament — a C++ port of the original
/// `TournamentSettings`. For now this only covers buy-in pricing and the rebuy
/// cap; prize payouts and other tournament features are still to be designed.
struct TournamentConfig {
    TournamentPricing pricing = TournamentPricing::ArcVsNonArc;
    bool arcOnly = false;   // if true, non-ARC players cannot take part
    int rebuyCap = 0;       // max rebuys per player; 0 = unlimited

    // ARC vs Non-ARC pricing (cents).
    std::int64_t arcBuyInCents = 1000;
    std::int64_t nonArcBuyInCents = 1500;
    std::int64_t arcRebuyCents = 1000;
    std::int64_t nonArcRebuyCents = 1500;

    // Membership-tier pricing (cents): Bronze .. Diamond.
    std::int64_t bronzeBuyInCents = 2500;
    std::int64_t silverBuyInCents = 2300;
    std::int64_t goldBuyInCents = 2300;
    std::int64_t platinumBuyInCents = 2300;
    std::int64_t diamondBuyInCents = 2300;

    std::int64_t bronzeRebuyCents = 2500;
    std::int64_t silverRebuyCents = 2500;
    std::int64_t goldRebuyCents = 2500;
    std::int64_t platinumRebuyCents = 2500;
    std::int64_t diamondRebuyCents = 2500;

    bool usesMembershipPricing() const noexcept {
        return pricing == TournamentPricing::MembershipTiers;
    }

    /// Is a further buy-in allowed given how many the player already made?
    /// (First buy-in is always allowed; the cap limits rebuys.)
    bool canBuyIn(int existingBuyInCount) const noexcept {
        return rebuyCap <= 0 || existingBuyInCount < 1 + rebuyCap;
    }

    /// The buy-in price for this player (membership tier or ARC status).
    Money priceFor(const Player& player, bool isRebuy) const;

    /// A human label for the buy-in, e.g. "ARC Member Rebuy" or "Gold Buy-in".
    std::string labelFor(const Player& player, bool isRebuy) const;

    /// Can this player take part at all? (ARC-only tournaments exclude non-ARC.)
    bool eligible(const Player& player) const;

    /// Short description of the pricing structure, for the UI.
    std::string pricingSummary() const;
};

} // namespace pokersoc::domain
