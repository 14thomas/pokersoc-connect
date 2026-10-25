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

/// Parse free-text chip amounts ("25", "1.5K", "150k", "1,000"). Returns -1 if invalid.
long long parseChipAmount(std::string_view text) noexcept;
/// Compact display for a numeric chip amount (1500 -> "1.5K", 150000 -> "150K").
std::string formatChipAmount(long long value);

/// One blind level on the tournament clock (chip units, not cash).
/// A break keeps the previous blinds and only runs the clock.
/// Blind amounts are free text so values like "1.5K" / "150K" are allowed.
struct BlindLevel {
    std::string smallBlind = "25";
    std::string bigBlind = "50";
    std::string ante; // empty / "0" / "—" = no big blind ante
    int durationMinutes = 15;
    bool isBreak = false;

    /// "100 / 200", "1.5K / 3K", or "Break".
    std::string blindsLabel() const;
    /// "—" when there is no big blind ante, otherwise the ante text as entered.
    std::string anteLabel() const;
    bool hasAnte() const noexcept;
};

/// A finishing place's share of the live prize pool.
struct PrizeShare {
    int place = 1;
    double percent = 50;
    /// Rounding leftover is added to this place. The field-size file sets it on 1st.
    bool shareLeftover = false;

    Money amountFrom(Money pool) const noexcept;
};

/// A place after rounding (and leftover, if enabled).
struct PrizePayout {
    int place = 1;
    double percent = 0;
    Money amount;
};

/// One field-size band: these percents apply when the entry count is in range.
struct PrizeBand {
    int minPlayers = 1;
    int maxPlayers = 8;
    std::vector<PrizeShare> prizes;
};

/// A named prize schedule. Constant schedules ignore the field size.
/// Variable schedules pick a band from how many buy-ins and rebuys count.
struct PrizePreset {
    std::string name = "Prize preset";
    bool variableByPlayers = false;
    bool countBuyins = true;
    bool countRebuys = false;
    std::vector<PrizeShare> prizes;
    std::vector<PrizeBand> levels;

    int fieldSize(int buyIns, int rebuys) const noexcept;
    /// Index of the band that applies at this field size, or -1 when this preset is constant.
    int bandIndexFor(int players) const noexcept;
    const std::vector<PrizeShare>& sharesFor(int players) const;

    /// The file shape `{ "prizeConfig": { ... } }`.
    std::string toJson() const;
    /// Accepts that file, a bare prizeConfig object, or an old `[{place, pct}]` list.
    static PrizePreset fromJson(const std::string& text);
    /// One preset, or every entry of a `{ "presets": [ ... ] }` library.
    static std::vector<PrizePreset> listFromJson(const std::string& text);
};

/// A chip denomination shown on the TV board.
/// imageFile is either a filename under Assets/Chips/ or an absolute filepath.
/// value is free text (25, 1.5K, 100K, 1M) — same parsing as blind amounts.
struct BoardChip {
    std::string imageFile;
    std::string value;
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
    std::vector<PrizePreset> prizePresets;
    int activePrizePreset = 0;
    /// Saved chip list. The TV board shows the six lowest values.
    std::vector<BoardChip> boardChips;

    PrizeRounding prizeRounding = PrizeRounding::Exact;
    bool leftoverToFirst = true;

    std::string name = "Tournament";
    int startingStack = 10000;
    /// 1-based playing level (breaks skipped) at which registration closes.
    /// 0 = registration stays open.
    int registrationEndsLevel = 4;

    /// Individual TV-board side-column items. Centre clock/blinds always show.
    struct BoardDisplay {
        bool registrationEnd = true;
        bool level = true;
        bool timeToBreak = true;
        bool breakLength = true;
        bool avgStack = true;
        bool players = true;
        bool reentries = true;
        bool total = true;
        bool remaining = true;
    } board;

    TournamentConfig();

    static std::vector<BlindLevel> defaultBlinds();
    static std::vector<PrizeShare> defaultPrizes();
    /// "By field size" (the shipped schedule) and "Fixed places" (50 / 30 / 20).
    static std::vector<PrizePreset> defaultPrizePresets();

    const PrizePreset& prizePreset() const;
    PrizePreset& prizePreset();

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

    /// Place amounts after rounding for the active preset at this field size.
    /// `buyIns` and `rebuys` are counted only when the preset says so.
    /// Unallocated percent stays in the pool. Rounding leftover goes to a place
    /// marked shareLeftover, or to 1st when leftoverToFirst is set.
    std::vector<PrizePayout> prizePayouts(Money pool, int buyIns, int rebuys) const;

    /// Compact array for AppStore persistence.
    std::string blindsJson() const;
    /// File format: `{ "rounds": [ { "minutes", "sb", "bb", "bb_ante" } | { "minutes", "break": true } ] }`.
    std::string blindsStructureJson() const;
    /// Library document: `{ "active": 0, "presets": [ { "prizeConfig": ... } ] }`.
    std::string prizePresetsJson() const;
    /// Old flat list, kept so a session saved before presets still opens.
    std::string prizesJson() const;
    std::string boardChipsJson() const;
    /// Accepts the AppStore array or a `{ "rounds": [...] }` structure file.
    void setBlindsFromJson(const std::string& text);
    /// Flat `[{place, pct}]` list. Replaces the fixed preset and selects it.
    void setPrizesFromJson(const std::string& text);
    void setPrizePresetsFromJson(const std::string& text);
    void setBoardChipsFromJson(const std::string& text);

    const BlindLevel* levelAt(int index) const noexcept;
    /// Blinds in force at `index`: the row itself, or the last playing level before a break.
    const BlindLevel* activeBlinds(int index) const noexcept;

    int playingLevelCount() const noexcept;
    /// 1-based playing-level number at `index`, or 0 when that row is a break.
    int playingLevelNumber(int index) const noexcept;
    /// Structure index of the Nth playing level (1-based), or -1.
    int indexOfPlayingLevel(int number) const noexcept;
};

} // namespace pokersoc::domain
