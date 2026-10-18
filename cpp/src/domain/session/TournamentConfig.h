#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "domain/money/Money.h"

namespace pokersoc::domain {

class Player;

/// How a tournament prices its buy-ins (ported from the original app's
/// TournamentSettings.PricingStructure).
enum class TournamentPricing { ArcVsNonArc, MembershipTiers };

const char* tournamentPricingName(TournamentPricing pricing) noexcept;
TournamentPricing tournamentPricingFromName(std::string_view name) noexcept;

/// How place amounts are rounded off the live prize pool.
enum class PrizeRounding {
    Exact,
    NearestFiveCents,
    NearestDollar,
    NearestFive,
    NearestTen,
    DownDollar,
    DownFive,
};

const char* prizeRoundingName(PrizeRounding rounding) noexcept;
PrizeRounding prizeRoundingFromName(std::string_view name) noexcept;

/// One blind level on the tournament clock (chip units, not cash).
/// A break keeps the previous blinds and only runs the clock.
struct BlindLevel {
    int smallBlind = 25;
    int bigBlind = 50;
    int ante = 0;
    int durationMinutes = 15;
    bool isBreak = false;

    /// "100 / 200", or "Break".
    std::string blindsLabel() const;
    /// "—" when there is no big blind ante, otherwise a grouped number.
    std::string anteLabel() const;
};

/// A finishing place's share of the live prize pool.
struct PrizeShare {
    int place = 1;
    int percent = 50;

    Money amountFrom(Money pool) const noexcept;
};

/// A place after rounding (and leftover-to-first, if enabled).
struct PrizePayout {
    int place = 1;
    int percent = 0;
    Money amount;
};

/// Bounty and rake taken from one buy-in or rebuy.
struct BuyInFees {
    std::int64_t bountyCents = 0;
    std::int64_t rakeCents = 0;
};

/// How one buy-in/rebuy is carved up.
struct BuyInSplit {
    Money prize;
    Money bounty;
    Money rake;
};

/// The tunable settings of a tournament — a C++ port of the original
/// `TournamentSettings`, plus the room-display clock (blinds and prizing).
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

    BuyInFees arcFees;
    BuyInFees nonArcFees;
    BuyInFees arcRebuyFees;
    BuyInFees nonArcRebuyFees;

    BuyInFees bronzeFees;
    BuyInFees silverFees;
    BuyInFees goldFees;
    BuyInFees platinumFees;
    BuyInFees diamondFees;

    BuyInFees bronzeRebuyFees;
    BuyInFees silverRebuyFees;
    BuyInFees goldRebuyFees;
    BuyInFees platinumRebuyFees;
    BuyInFees diamondRebuyFees;

    std::vector<BlindLevel> blinds;
    std::vector<PrizeShare> prizes;

    PrizeRounding prizeRounding = PrizeRounding::Exact;
    bool leftoverToFirst = true;

    TournamentConfig();

    static std::vector<BlindLevel> defaultBlinds();
    static std::vector<PrizeShare> defaultPrizes();

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

    /// Bounty and rake for this player's buy-in or rebuy.
    const BuyInFees& feesFor(const Player& player, bool isRebuy) const;

    /// Split this player's buy-in into prize / bounty / rake. Rake is taken
    /// first, then bounty; anything left is prize pool.
    BuyInSplit split(const Player& player, bool isRebuy) const;

    /// True if any active pricing row has a bounty.
    bool hasBounties() const;

    /// Smallest positive bounty among active pricing rows, or 0.
    std::int64_t typicalBountyCents() const;

    /// Per-bounty amount when this player redeems knockouts: their buy-in
    /// bounty, else the typical bounty on this event.
    Money bountyUnit(const Player& player) const;

    /// Place amounts after rounding. Unallocated percent stays in the pool;
    /// leftover from rounding can be given to 1st.
    std::vector<PrizePayout> prizePayouts(Money pool) const;

    std::string blindsJson() const;
    std::string prizesJson() const;
    void setBlindsFromJson(const std::string& text);
    void setPrizesFromJson(const std::string& text);

    const BlindLevel* levelAt(int index) const noexcept;
    /// Blinds in force at `index`: the row itself, or the last playing level before a break.
    const BlindLevel* activeBlinds(int index) const noexcept;
};

} // namespace pokersoc::domain
