#include "domain/session/TournamentConfig.h"

#include <algorithm>

#include "domain/player/Player.h"
#include "util/Json.h"

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

const char* prizeRoundingName(PrizeRounding rounding) noexcept {
    switch (rounding) {
        case PrizeRounding::NearestFiveCents: return "NEAREST_5C";
        case PrizeRounding::NearestDollar:    return "NEAREST_1";
        case PrizeRounding::NearestFive:      return "NEAREST_5";
        case PrizeRounding::NearestTen:       return "NEAREST_10";
        case PrizeRounding::DownDollar:       return "DOWN_1";
        case PrizeRounding::DownFive:         return "DOWN_5";
        case PrizeRounding::Exact:            return "EXACT";
    }
    return "EXACT";
}

PrizeRounding prizeRoundingFromName(std::string_view name) noexcept {
    if (name == "NEAREST_5C") return PrizeRounding::NearestFiveCents;
    if (name == "NEAREST_1")  return PrizeRounding::NearestDollar;
    if (name == "NEAREST_5")  return PrizeRounding::NearestFive;
    if (name == "NEAREST_10") return PrizeRounding::NearestTen;
    if (name == "DOWN_1")     return PrizeRounding::DownDollar;
    if (name == "DOWN_5")     return PrizeRounding::DownFive;
    return PrizeRounding::Exact;
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

namespace {

BuyInSplit applySplit(Money buyIn, const BuyInFees& fees) noexcept {
    BuyInSplit result;
    std::int64_t remaining = buyIn.cents();
    if (remaining < 0) remaining = 0;
    const std::int64_t rake = std::min(fees.rakeCents < 0 ? 0 : fees.rakeCents, remaining);
    remaining -= rake;
    const std::int64_t bounty = std::min(fees.bountyCents < 0 ? 0 : fees.bountyCents, remaining);
    remaining -= bounty;
    result.rake = Money::fromCents(rake);
    result.bounty = Money::fromCents(bounty);
    result.prize = Money::fromCents(remaining);
    return result;
}

Money applyPrizeRounding(Money amount, PrizeRounding rounding) noexcept {
    const std::int64_t cents = amount.cents();
    if (cents <= 0) return Money{};
    auto nearest = [](std::int64_t value, std::int64_t step) {
        return ((value + step / 2) / step) * step;
    };
    auto down = [](std::int64_t value, std::int64_t step) { return (value / step) * step; };
    switch (rounding) {
        case PrizeRounding::NearestFiveCents: return Money::fromCents(nearest(cents, 5));
        case PrizeRounding::NearestDollar:    return Money::fromCents(nearest(cents, 100));
        case PrizeRounding::NearestFive:      return Money::fromCents(nearest(cents, 500));
        case PrizeRounding::NearestTen:       return Money::fromCents(nearest(cents, 1000));
        case PrizeRounding::DownDollar:       return Money::fromCents(down(cents, 100));
        case PrizeRounding::DownFive:         return Money::fromCents(down(cents, 500));
        case PrizeRounding::Exact:            return amount;
    }
    return amount;
}

} // namespace

const BuyInFees& TournamentConfig::feesFor(const Player& player, bool isRebuy) const {
    if (usesMembershipPricing()) {
        switch (player.membership()) {
            case MembershipType::Silver:
                return isRebuy ? silverRebuyFees : silverFees;
            case MembershipType::Gold:
                return isRebuy ? goldRebuyFees : goldFees;
            case MembershipType::Platinum:
                return isRebuy ? platinumRebuyFees : platinumFees;
            case MembershipType::Diamond:
                return isRebuy ? diamondRebuyFees : diamondFees;
            case MembershipType::Bronze:
            case MembershipType::None:
                return isRebuy ? bronzeRebuyFees : bronzeFees;
        }
        return isRebuy ? bronzeRebuyFees : bronzeFees;
    }

    const bool isArc = player.arcStatus() == ArcStatus::Arc;
    if (isArc) return isRebuy ? arcRebuyFees : arcFees;
    return isRebuy ? nonArcRebuyFees : nonArcFees;
}

BuyInSplit TournamentConfig::split(const Player& player, bool isRebuy) const {
    return applySplit(priceFor(player, isRebuy), feesFor(player, isRebuy));
}

bool TournamentConfig::hasBounties() const {
    return typicalBountyCents() > 0;
}

std::int64_t TournamentConfig::typicalBountyCents() const {
    auto consider = [](std::int64_t smallest, const BuyInFees& fees) {
        if (fees.bountyCents <= 0) return smallest;
        if (smallest == 0 || fees.bountyCents < smallest) return fees.bountyCents;
        return smallest;
    };
    std::int64_t smallest = 0;
    if (usesMembershipPricing()) {
        smallest = consider(smallest, bronzeFees);
        smallest = consider(smallest, silverFees);
        smallest = consider(smallest, goldFees);
        smallest = consider(smallest, platinumFees);
        smallest = consider(smallest, diamondFees);
        smallest = consider(smallest, bronzeRebuyFees);
        smallest = consider(smallest, silverRebuyFees);
        smallest = consider(smallest, goldRebuyFees);
        smallest = consider(smallest, platinumRebuyFees);
        smallest = consider(smallest, diamondRebuyFees);
    } else {
        smallest = consider(smallest, arcFees);
        smallest = consider(smallest, arcRebuyFees);
        if (!arcOnly) {
            smallest = consider(smallest, nonArcFees);
            smallest = consider(smallest, nonArcRebuyFees);
        }
    }
    return smallest;
}

Money TournamentConfig::bountyUnit(const Player& player) const {
    std::int64_t cents = feesFor(player, false).bountyCents;
    if (cents <= 0) cents = typicalBountyCents();
    return Money::fromCents(cents < 0 ? 0 : cents);
}

std::vector<PrizePayout> TournamentConfig::prizePayouts(Money pool) const {
    std::vector<PrizePayout> result;
    result.reserve(prizes.size());
    std::int64_t roundedSum = 0;
    std::int64_t allocated = 0;
    int firstIndex = -1;
    int firstPlace = 0;
    for (const PrizeShare& prize : prizes) {
        PrizePayout row;
        row.place = prize.place;
        row.percent = prize.percent;
        const Money raw = prize.amountFrom(pool);
        allocated += raw.cents();
        row.amount = applyPrizeRounding(raw, prizeRounding);
        roundedSum += row.amount.cents();
        if (firstIndex < 0 || row.place < firstPlace) {
            firstPlace = row.place;
            firstIndex = static_cast<int>(result.size());
        }
        result.push_back(row);
    }
    if (leftoverToFirst && firstIndex >= 0) {
        const std::int64_t leftover = allocated - roundedSum;
        const std::int64_t first = result[static_cast<std::size_t>(firstIndex)].amount.cents() + leftover;
        result[static_cast<std::size_t>(firstIndex)].amount = Money::fromCents(first < 0 ? 0 : first);
    }
    return result;
}

namespace {

std::string groupedInt(int value) {
    const bool negative = value < 0;
    std::string digits = std::to_string(negative ? -value : value);
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) {
        digits.insert(static_cast<std::size_t>(i), ",");
    }
    return negative ? "-" + digits : digits;
}

int jsonInt(const util::JsonValue& object, const char* key, int fallback) {
    const util::JsonValue* value = object.get(key);
    if (!value || value->kind != util::JsonValue::Kind::Number) return fallback;
    return static_cast<int>(value->number);
}

} // namespace

std::string BlindLevel::blindsLabel() const {
    if (isBreak) return "Break";
    return groupedInt(smallBlind) + " / " + groupedInt(bigBlind);
}

std::string BlindLevel::anteLabel() const {
    return ante <= 0 ? "—" : groupedInt(ante);
}

Money PrizeShare::amountFrom(Money pool) const noexcept {
    if (percent <= 0) return Money{};
    return Money::fromCents(pool.cents() * percent / 100);
}

TournamentConfig::TournamentConfig() : blinds(defaultBlinds()), prizes(defaultPrizes()) {}

std::vector<BlindLevel> TournamentConfig::defaultBlinds() {
    return {
        {25, 50, 0, 15},      {50, 100, 0, 15},     {75, 150, 0, 15},
        {100, 200, 25, 15},   {150, 300, 25, 15},   {200, 400, 50, 15},
        {300, 600, 75, 15},   {400, 800, 100, 15},  {500, 1000, 100, 15},
        {700, 1400, 200, 15}, {1000, 2000, 300, 15}, {2000, 4000, 500, 15},
    };
}

std::vector<PrizeShare> TournamentConfig::defaultPrizes() {
    return {{1, 50}, {2, 30}, {3, 20}};
}

std::string TournamentConfig::blindsJson() const {
    std::string out = "[";
    for (std::size_t i = 0; i < blinds.size(); ++i) {
        if (i > 0) out += ',';
        const BlindLevel& level = blinds[i];
        out += "{\"sb\":" + std::to_string(level.smallBlind) + ",\"bb\":" +
               std::to_string(level.bigBlind) + ",\"ante\":" + std::to_string(level.ante) +
               ",\"min\":" + std::to_string(level.durationMinutes);
        if (level.isBreak) out += ",\"break\":1";
        out += "}";
    }
    out += "]";
    return out;
}

std::string TournamentConfig::prizesJson() const {
    std::string out = "[";
    for (std::size_t i = 0; i < prizes.size(); ++i) {
        if (i > 0) out += ',';
        const PrizeShare& prize = prizes[i];
        out += "{\"place\":" + std::to_string(prize.place) + ",\"pct\":" +
               std::to_string(prize.percent) + "}";
    }
    out += "]";
    return out;
}

void TournamentConfig::setBlindsFromJson(const std::string& text) {
    if (text.empty()) return;
    try {
        const util::JsonValue root = util::parseJson(text);
        if (!root.isArray() || root.items.empty()) return;
        std::vector<BlindLevel> parsed;
        parsed.reserve(root.items.size());
        for (const util::JsonValue& item : root.items) {
            if (!item.isObject()) continue;
            BlindLevel level;
            level.smallBlind = jsonInt(item, "sb", level.smallBlind);
            level.bigBlind = jsonInt(item, "bb", level.bigBlind);
            level.ante = jsonInt(item, "ante", level.ante);
            level.durationMinutes = jsonInt(item, "min", level.durationMinutes);
            if (level.smallBlind < 0) level.smallBlind = 0;
            if (level.bigBlind < 0) level.bigBlind = 0;
            if (level.ante < 0) level.ante = 0;
            if (level.durationMinutes < 1) level.durationMinutes = 15;
            if (const util::JsonValue* brk = item.get("break")) {
                level.isBreak = brk->kind == util::JsonValue::Kind::Bool
                                    ? brk->boolean
                                    : brk->kind == util::JsonValue::Kind::Number && brk->number != 0;
            }
            parsed.push_back(level);
        }
        if (!parsed.empty()) blinds = std::move(parsed);
    } catch (...) {
        // Keep the constructor defaults if the stored JSON is unreadable.
    }
}

void TournamentConfig::setPrizesFromJson(const std::string& text) {
    if (text.empty()) return;
    try {
        const util::JsonValue root = util::parseJson(text);
        if (!root.isArray() || root.items.empty()) return;
        std::vector<PrizeShare> parsed;
        parsed.reserve(root.items.size());
        for (const util::JsonValue& item : root.items) {
            if (!item.isObject()) continue;
            PrizeShare prize;
            prize.place = jsonInt(item, "place", prize.place);
            prize.percent = jsonInt(item, "pct", prize.percent);
            if (prize.place < 1) prize.place = 1;
            if (prize.percent < 0) prize.percent = 0;
            parsed.push_back(prize);
        }
        if (!parsed.empty()) prizes = std::move(parsed);
    } catch (...) {
        // Keep the constructor defaults if the stored JSON is unreadable.
    }
}

const BlindLevel* TournamentConfig::levelAt(int index) const noexcept {
    if (index < 0 || index >= static_cast<int>(blinds.size())) return nullptr;
    return &blinds[static_cast<std::size_t>(index)];
}

const BlindLevel* TournamentConfig::activeBlinds(int index) const noexcept {
    for (int i = index; i >= 0; --i) {
        const BlindLevel* level = levelAt(i);
        if (level && !level->isBreak) return level;
    }
    return levelAt(index);
}

} // namespace pokersoc::domain
