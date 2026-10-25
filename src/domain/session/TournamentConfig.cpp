#include "domain/session/TournamentConfig.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "domain/player/Player.h"
#include "util/Json.h"

namespace pokersoc::domain {

static const char kFieldSizePrizeJson[] =
#include "default_prize_config.inc"
;

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

std::vector<PrizePayout> TournamentConfig::prizePayouts(Money pool, int buyIns, int rebuys) const {
    const PrizePreset& preset = prizePreset();
    const std::vector<PrizeShare>& shares = preset.sharesFor(preset.fieldSize(buyIns, rebuys));
    std::vector<PrizePayout> result;
    result.reserve(shares.size());
    std::int64_t roundedSum = 0;
    std::int64_t allocated = 0;
    int firstIndex = -1;
    int firstPlace = 0;
    int leftoverIndex = -1;
    for (const PrizeShare& prize : shares) {
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
        if (prize.shareLeftover && leftoverIndex < 0) {
            leftoverIndex = static_cast<int>(result.size());
        }
        result.push_back(row);
    }
    if (leftoverIndex < 0 && leftoverToFirst) leftoverIndex = firstIndex;
    if (leftoverIndex >= 0) {
        const std::int64_t leftover = allocated - roundedSum;
        const std::size_t index = static_cast<std::size_t>(leftoverIndex);
        const std::int64_t adjusted = result[index].amount.cents() + leftover;
        result[index].amount = Money::fromCents(adjusted < 0 ? 0 : adjusted);
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

long long parseChipAmount(std::string_view text) noexcept {
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char ch : text) {
        if (ch == ',' || ch == ' ' || ch == '\t') continue;
        cleaned.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    if (cleaned.empty() || cleaned == "-" || cleaned == "—" || cleaned == "--") return -1;

    double multiplier = 1.0;
    if (!cleaned.empty() && (cleaned.back() == 'k' || cleaned.back() == 'm')) {
        multiplier = cleaned.back() == 'm' ? 1'000'000.0 : 1'000.0;
        cleaned.pop_back();
    }
    if (cleaned.empty()) return -1;

    char* end = nullptr;
    const double value = std::strtod(cleaned.c_str(), &end);
    if (end == cleaned.c_str() || (end && *end != '\0') || value < 0) return -1;
    const double scaled = value * multiplier;
    if (scaled > static_cast<double>(LLONG_MAX)) return -1;
    return static_cast<long long>(scaled + 0.5);
}

std::string formatChipAmount(long long value) {
    if (value < 0) value = 0;
    if (value >= 1'000'000 && value % 1'000'000 == 0) {
        return std::to_string(value / 1'000'000) + "M";
    }
    if (value >= 1000 && value % 1000 == 0) {
        return std::to_string(value / 1000) + "K";
    }
    if (value >= 1000 && value % 100 == 0) {
        const long long whole = value / 1000;
        const long long frac = (value % 1000) / 100;
        return std::to_string(whole) + "." + std::to_string(frac) + "K";
    }
    return groupedInt(static_cast<int>(std::min<long long>(value, INT_MAX)));
}

std::string BlindLevel::blindsLabel() const {
    if (isBreak) return "Break";
    return smallBlind + " / " + bigBlind;
}

std::string BlindLevel::anteLabel() const {
    return hasAnte() ? ante : "—";
}

bool BlindLevel::hasAnte() const noexcept {
    if (ante.empty()) return false;
    std::string trimmed;
    for (char ch : ante) {
        if (ch != ' ' && ch != '\t') trimmed.push_back(ch);
    }
    if (trimmed.empty() || trimmed == "-" || trimmed == "—" || trimmed == "--" || trimmed == "0") {
        return false;
    }
    const long long value = parseChipAmount(trimmed);
    return value != 0; // -1 (unparsed text like "BB") still counts as present
}

Money PrizeShare::amountFrom(Money pool) const noexcept {
    if (percent <= 0 || pool.cents() <= 0) return Money{};
    const double cents = static_cast<double>(pool.cents()) * percent / 100.0;
    return Money::fromCents(static_cast<std::int64_t>(std::llround(cents)));
}

TournamentConfig::TournamentConfig()
    : blinds(defaultBlinds()), prizePresets(defaultPrizePresets()) {}

std::vector<BlindLevel> TournamentConfig::defaultBlinds() {
    // PokerSoc ~5h structure (Assets/BlindStructures/PokerSoc_rounds_5h.json).
    const auto ante = [](long long value) {
        return value > 0 ? formatChipAmount(value) : std::string{};
    };
    return {
        {formatChipAmount(100), formatChipAmount(100), ante(0), 15},
        {formatChipAmount(100), formatChipAmount(200), ante(0), 15},
        {formatChipAmount(200), formatChipAmount(300), ante(0), 15},
        {formatChipAmount(200), formatChipAmount(400), ante(0), 15},
        {formatChipAmount(300), formatChipAmount(600), ante(600), 15},
        {formatChipAmount(400), formatChipAmount(800), ante(800), 15},
        {"", "", "", 15, true},
        {formatChipAmount(500), formatChipAmount(1000), ante(1000), 15},
        {formatChipAmount(500), formatChipAmount(1500), ante(1500), 15},
        {formatChipAmount(1000), formatChipAmount(2000), ante(2000), 15},
        {formatChipAmount(1500), formatChipAmount(3000), ante(3000), 15},
        {formatChipAmount(2000), formatChipAmount(4000), ante(4000), 15},
        {formatChipAmount(3000), formatChipAmount(6000), ante(6000), 15},
        {formatChipAmount(5000), formatChipAmount(10000), ante(10000), 15},
        {"", "", "", 15, true},
        {formatChipAmount(10000), formatChipAmount(20000), ante(20000), 15},
        {formatChipAmount(15000), formatChipAmount(30000), ante(30000), 15},
        {formatChipAmount(20000), formatChipAmount(40000), ante(40000), 15},
        {formatChipAmount(30000), formatChipAmount(60000), ante(60000), 15},
        {formatChipAmount(50000), formatChipAmount(100000), ante(100000), 15},
        {formatChipAmount(75000), formatChipAmount(150000), ante(150000), 15},
    };
}

std::vector<PrizeShare> TournamentConfig::defaultPrizes() {
    return {{1, 50, true}, {2, 30, false}, {3, 20, false}};
}

std::string TournamentConfig::blindsJson() const {
    std::string out = "[";
    for (std::size_t i = 0; i < blinds.size(); ++i) {
        if (i > 0) out += ',';
        const BlindLevel& level = blinds[i];
        out += "{\"sb\":" + util::jsonString(level.smallBlind) + ",\"bb\":" +
               util::jsonString(level.bigBlind) + ",\"ante\":" + util::jsonString(level.ante) +
               ",\"min\":" + std::to_string(level.durationMinutes);
        if (level.isBreak) out += ",\"break\":1";
        out += "}";
    }
    out += "]";
    return out;
}

namespace {

std::string jsonChipNumberOrString(const std::string& text) {
    const long long value = parseChipAmount(text);
    if (value >= 0) return std::to_string(value);
    return util::jsonString(text);
}

} // namespace

std::string TournamentConfig::blindsStructureJson() const {
    std::string out = "{\n  \"rounds\": [\n";
    for (std::size_t i = 0; i < blinds.size(); ++i) {
        if (i > 0) out += ",\n";
        const BlindLevel& level = blinds[i];
        out += "    { \"minutes\": " + std::to_string(level.durationMinutes);
        if (level.isBreak) {
            out += ", \"break\": true }";
            continue;
        }
        out += ", \"sb\": " + jsonChipNumberOrString(level.smallBlind);
        out += ", \"bb\": " + jsonChipNumberOrString(level.bigBlind);
        const long long ante = parseChipAmount(level.ante);
        out += ", \"bb_ante\": " + std::to_string(ante > 0 ? ante : 0);
        out += " }";
    }
    out += "\n  ]\n}\n";
    return out;
}

namespace {

std::string jsonPercent(double percent) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", percent);
    return buffer;
}

std::string jsonPrizeList(const std::vector<PrizeShare>& prizes, int indent) {
    const std::string pad(static_cast<std::size_t>(indent), ' ');
    const std::string inner(static_cast<std::size_t>(indent + 2), ' ');
    std::string out = "[\n";
    for (std::size_t i = 0; i < prizes.size(); ++i) {
        if (i > 0) out += ",\n";
        const PrizeShare& prize = prizes[i];
        out += inner + "{ \"rank\": " + std::to_string(prize.place) +
               ", \"percent\": " + jsonPercent(prize.percent);
        if (prize.shareLeftover) out += ", \"shareLeftover\": true";
        out += " }";
    }
    out += "\n" + pad + "]";
    return out;
}

bool jsonBool(const util::JsonValue& object, const char* key, bool fallback) {
    const util::JsonValue* value = object.get(key);
    if (!value) return fallback;
    if (value->kind == util::JsonValue::Kind::Bool) return value->boolean;
    if (value->kind == util::JsonValue::Kind::Number) return value->number != 0;
    return fallback;
}

double jsonNumber(const util::JsonValue& object, const char* key, double fallback) {
    const util::JsonValue* value = object.get(key);
    if (!value || value->kind != util::JsonValue::Kind::Number) return fallback;
    return value->number;
}

std::string jsonText(const util::JsonValue& object, const char* key) {
    const util::JsonValue* value = object.get(key);
    if (!value || value->kind != util::JsonValue::Kind::String) return {};
    return value->string;
}

std::vector<PrizeShare> readPrizeShares(const util::JsonValue& array) {
    std::vector<PrizeShare> parsed;
    if (!array.isArray()) return parsed;
    for (const util::JsonValue& item : array.items) {
        if (!item.isObject()) continue;
        PrizeShare prize;
        prize.place = static_cast<int>(jsonNumber(item, "rank", jsonNumber(item, "place", prize.place)));
        prize.percent = jsonNumber(item, "percent", jsonNumber(item, "pct", prize.percent));
        prize.shareLeftover = jsonBool(item, "shareLeftover", false);
        if (prize.place < 1) prize.place = 1;
        if (prize.percent < 0) prize.percent = 0;
        if (prize.percent > 100) prize.percent = 100;
        parsed.push_back(prize);
    }
    return parsed;
}

} // namespace

std::string TournamentConfig::prizesJson() const {
    const std::vector<PrizeShare>& shares = prizePreset().variableByPlayers
                                                 ? prizePreset().sharesFor(0)
                                                 : prizePreset().prizes;
    std::string out = "[";
    for (std::size_t i = 0; i < shares.size(); ++i) {
        if (i > 0) out += ',';
        const PrizeShare& prize = shares[i];
        out += "{\"place\":" + std::to_string(prize.place) + ",\"pct\":" + jsonPercent(prize.percent) +
               "}";
    }
    out += "]";
    return out;
}

std::string TournamentConfig::prizePresetsJson() const {
    std::string out = "{\n  \"active\": " + std::to_string(activePrizePreset) + ",\n  \"presets\": [\n";
    for (std::size_t i = 0; i < prizePresets.size(); ++i) {
        if (i > 0) out += ",\n";
        const std::string body = prizePresets[i].toJson();
        // Indent the preset document under the library list.
        std::string indented = "    ";
        for (char ch : body) {
            indented.push_back(ch);
            if (ch == '\n') indented += "    ";
        }
        out += indented;
    }
    out += "\n  ]\n}\n";
    return out;
}

std::string TournamentConfig::boardChipsJson() const {
    std::string out = "[";
    for (std::size_t i = 0; i < boardChips.size(); ++i) {
        if (i > 0) out += ',';
        const BoardChip& chip = boardChips[i];
        out += "{\"img\":" + util::jsonString(chip.imageFile) +
               ",\"val\":" + util::jsonString(chip.value) + "}";
    }
    out += "]";
    return out;
}

void TournamentConfig::setBlindsFromJson(const std::string& text) {
    if (text.empty()) return;
    try {
        const util::JsonValue root = util::parseJson(text);
        const util::JsonValue* rounds = nullptr;
        if (root.isArray()) {
            rounds = &root;
        } else if (root.isObject()) {
            if (const util::JsonValue* keyed = root.get("rounds"); keyed && keyed->isArray()) {
                rounds = keyed;
            } else if (const util::JsonValue* keyed = root.get("blinds"); keyed && keyed->isArray()) {
                rounds = keyed;
            }
        }
        if (!rounds || rounds->items.empty()) return;

        std::vector<BlindLevel> parsed;
        parsed.reserve(rounds->items.size());
        const auto jsonChip = [](const util::JsonValue& object, const char* key,
                                 const std::string& fallback) {
            const util::JsonValue* value = object.get(key);
            if (!value) return fallback;
            if (value->kind == util::JsonValue::Kind::String) return value->string;
            if (value->kind == util::JsonValue::Kind::Number) {
                const long long n = static_cast<long long>(value->number);
                if (n <= 0) return std::string{};
                return formatChipAmount(n);
            }
            return fallback;
        };
        for (const util::JsonValue& item : rounds->items) {
            if (!item.isObject()) continue;
            BlindLevel level;
            if (const util::JsonValue* brk = item.get("break")) {
                level.isBreak = brk->kind == util::JsonValue::Kind::Bool
                                    ? brk->boolean
                                    : brk->kind == util::JsonValue::Kind::Number && brk->number != 0;
            }
            level.durationMinutes = jsonInt(item, "minutes", jsonInt(item, "min", level.durationMinutes));
            if (level.durationMinutes < 1) level.durationMinutes = 15;
            if (!level.isBreak) {
                level.smallBlind = jsonChip(item, "sb", level.smallBlind);
                level.bigBlind = jsonChip(item, "bb", level.bigBlind);
                // File format uses bb_ante; AppStore uses ante.
                level.ante = jsonChip(item, "bb_ante", jsonChip(item, "ante", level.ante));
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
        const std::vector<PrizeShare> parsed = readPrizeShares(root);
        if (parsed.empty()) return;
        if (prizePresets.size() < 2) prizePresets = defaultPrizePresets();
        PrizePreset& fixed = prizePresets[1];
        fixed.name = "Fixed places";
        fixed.variableByPlayers = false;
        fixed.prizes = parsed;
        if (!fixed.prizes.empty() && !fixed.prizes.front().shareLeftover) {
            fixed.prizes.front().shareLeftover = true;
        }
        activePrizePreset = 1;
    } catch (...) {
        // Keep the constructor defaults if the stored JSON is unreadable.
    }
}

namespace {

PrizePreset presetFromConfig(const util::JsonValue& source) {
    const util::JsonValue* config = &source;
    if (source.isObject()) {
        if (const util::JsonValue* inner = source.get("prizeConfig"); inner && inner->isObject()) {
            config = inner;
        }
    }
    PrizePreset preset;
    if (!config->isObject()) return preset;
    preset.name = jsonText(*config, "name");
    preset.variableByPlayers = jsonBool(*config, "variableByPlayers", false);
    if (const util::JsonValue* selection = config->get("levelSelection");
        selection && selection->isObject()) {
        preset.countBuyins = jsonBool(*selection, "countBuyins", true);
        preset.countRebuys = jsonBool(*selection, "countRebuys", false);
    }
    if (const util::JsonValue* levels = config->get("prizeLevels"); levels && levels->isArray()) {
        for (const util::JsonValue& level : levels->items) {
            if (!level.isObject()) continue;
            PrizeBand band;
            band.minPlayers = static_cast<int>(jsonNumber(level, "minPlayers", 1));
            band.maxPlayers = static_cast<int>(jsonNumber(level, "maxPlayers", 0));
            if (band.minPlayers < 0) band.minPlayers = 0;
            if (const util::JsonValue* prizes = level.get("prizes")) {
                band.prizes = readPrizeShares(*prizes);
            }
            if (!band.prizes.empty()) preset.levels.push_back(std::move(band));
        }
        if (!preset.levels.empty()) preset.variableByPlayers = true;
    }
    if (const util::JsonValue* prizes = config->get("prizes"); prizes && prizes->isArray()) {
        preset.prizes = readPrizeShares(*prizes);
    }
    return preset;
}

} // namespace

void TournamentConfig::setPrizePresetsFromJson(const std::string& text) {
    if (text.empty()) return;
    try {
        const util::JsonValue root = util::parseJson(text);
        const util::JsonValue* list = nullptr;
        if (root.isObject()) {
            if (const util::JsonValue* presets = root.get("presets"); presets && presets->isArray()) {
                list = presets;
            }
        }
        if (!list || list->items.empty()) return;
        std::vector<PrizePreset> parsed;
        for (const util::JsonValue& item : list->items) {
            if (!item.isObject()) continue;
            PrizePreset preset = presetFromConfig(item);
            if (!preset.name.empty() || preset.variableByPlayers || !preset.prizes.empty() ||
                !preset.levels.empty()) {
                if (preset.name.empty()) preset.name = "Prize preset";
                parsed.push_back(std::move(preset));
            }
        }
        if (parsed.empty()) return;
        prizePresets = std::move(parsed);
        activePrizePreset = static_cast<int>(jsonNumber(root, "active", 0));
        if (activePrizePreset < 0 ||
            activePrizePreset >= static_cast<int>(prizePresets.size())) {
            activePrizePreset = 0;
        }
    } catch (...) {
        // Keep the constructor defaults if the stored JSON is unreadable.
    }
}

void TournamentConfig::setBoardChipsFromJson(const std::string& text) {
    boardChips.clear();
    if (text.empty()) return;
    try {
        const util::JsonValue root = util::parseJson(text);
        if (!root.isArray()) return;
        for (const util::JsonValue& item : root.items) {
            if (!item.isObject()) continue;
            BoardChip chip;
            if (const util::JsonValue* img = item.get("img");
                img && img->kind == util::JsonValue::Kind::String) {
                chip.imageFile = img->string;
            }
            if (const util::JsonValue* val = item.get("val")) {
                if (val->kind == util::JsonValue::Kind::String) {
                    chip.value = val->string;
                } else if (val->kind == util::JsonValue::Kind::Number) {
                    chip.value = formatChipAmount(static_cast<long long>(val->number));
                }
            }
            const long long numeric = parseChipAmount(chip.value);
            if (chip.imageFile.empty() && numeric <= 0) continue;
            boardChips.push_back(chip);
            if (boardChips.size() >= 6) break;
        }
        std::sort(boardChips.begin(), boardChips.end(), [](const BoardChip& a, const BoardChip& b) {
            return parseChipAmount(a.value) < parseChipAmount(b.value);
        });
    } catch (...) {
        boardChips.clear();
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

int TournamentConfig::playingLevelCount() const noexcept {
    int count = 0;
    for (const BlindLevel& level : blinds) {
        if (!level.isBreak) ++count;
    }
    return count;
}

int TournamentConfig::playingLevelNumber(int index) const noexcept {
    if (index < 0 || index >= static_cast<int>(blinds.size())) return 0;
    if (blinds[static_cast<std::size_t>(index)].isBreak) return 0;
    int number = 0;
    for (int i = 0; i <= index; ++i) {
        if (!blinds[static_cast<std::size_t>(i)].isBreak) ++number;
    }
    return number;
}

int PrizePreset::fieldSize(int buyIns, int rebuys) const noexcept {
    if (buyIns < 0) buyIns = 0;
    if (rebuys < 0) rebuys = 0;
    if (!countBuyins && !countRebuys) return buyIns;
    return (countBuyins ? buyIns : 0) + (countRebuys ? rebuys : 0);
}

int PrizePreset::bandIndexFor(int players) const noexcept {
    if (!variableByPlayers || levels.empty()) return -1;
    if (players <= 0) return 0;
    int chosen = 0;
    for (int i = 0; i < static_cast<int>(levels.size()); ++i) {
        const PrizeBand& band = levels[static_cast<std::size_t>(i)];
        if (players < band.minPlayers) break;
        chosen = i;
        if (band.maxPlayers <= 0 || players <= band.maxPlayers) break;
    }
    return chosen;
}

const std::vector<PrizeShare>& PrizePreset::sharesFor(int players) const {
    const int index = bandIndexFor(players);
    if (index < 0 || index >= static_cast<int>(levels.size())) return prizes;
    return levels[static_cast<std::size_t>(index)].prizes;
}

std::string PrizePreset::toJson() const {
    std::string out = "{\n  \"prizeConfig\": {\n";
    if (!name.empty()) out += "    \"name\": " + util::jsonString(name) + ",\n";
    out += std::string("    \"variableByPlayers\": ") + (variableByPlayers ? "true" : "false") + ",\n";
    if (variableByPlayers) {
        out += "    \"prizeLevels\": [\n";
        for (std::size_t i = 0; i < levels.size(); ++i) {
            if (i > 0) out += ",\n";
            const PrizeBand& band = levels[i];
            out += "      {\n";
            out += "        \"minPlayers\": " + std::to_string(band.minPlayers) + ",\n";
            out += "        \"maxPlayers\": " + std::to_string(band.maxPlayers) + ",\n";
            out += "        \"prizes\": " + jsonPrizeList(band.prizes, 8) + "\n";
            out += "      }";
        }
        out += "\n    ],\n";
    } else {
        out += "    \"prizes\": " + jsonPrizeList(prizes, 4) + ",\n";
    }
    out += "    \"levelSelection\": {\n";
    out += std::string("      \"countBuyins\": ") + (countBuyins ? "true" : "false") + ",\n";
    out += std::string("      \"countRebuys\": ") + (countRebuys ? "true" : "false") + "\n";
    out += "    }\n  }\n}\n";
    return out;
}

PrizePreset PrizePreset::fromJson(const std::string& text) {
    const std::vector<PrizePreset> list = listFromJson(text);
    if (list.empty()) return {};
    return list.front();
}

std::vector<PrizePreset> PrizePreset::listFromJson(const std::string& text) {
    std::vector<PrizePreset> parsed;
    if (text.empty()) return parsed;
    try {
        const util::JsonValue root = util::parseJson(text);
        if (root.isObject()) {
            if (const util::JsonValue* presets = root.get("presets");
                presets && presets->isArray() && !presets->items.empty()) {
                for (const util::JsonValue& item : presets->items) {
                    if (!item.isObject()) continue;
                    PrizePreset preset = presetFromConfig(item);
                    if (!preset.variableByPlayers && preset.prizes.empty() && preset.levels.empty()) continue;
                    if (preset.name.empty()) preset.name = "Prize preset";
                    parsed.push_back(std::move(preset));
                }
                if (!parsed.empty()) return parsed;
            }
        }
        if (root.isArray()) {
            PrizePreset preset;
            preset.name = "Fixed places";
            preset.prizes = readPrizeShares(root);
            if (!preset.prizes.empty()) parsed.push_back(std::move(preset));
            return parsed;
        }
        PrizePreset preset = presetFromConfig(root);
        if (preset.variableByPlayers || !preset.prizes.empty() || !preset.levels.empty()) {
            if (preset.name.empty()) preset.name = "Prize preset";
            parsed.push_back(std::move(preset));
        }
    } catch (...) {
        parsed.clear();
    }
    return parsed;
}

std::vector<PrizePreset> TournamentConfig::defaultPrizePresets() {
    std::vector<PrizePreset> presets;
    PrizePreset field = PrizePreset::fromJson(kFieldSizePrizeJson);
    field.name = "By field size";
    if (field.levels.empty()) {
        PrizeBand band;
        band.minPlayers = 1;
        band.maxPlayers = 8;
        band.prizes = {{1, 100, true}};
        field.levels.push_back(std::move(band));
        field.variableByPlayers = true;
        field.countBuyins = true;
        field.countRebuys = false;
    }
    presets.push_back(std::move(field));

    PrizePreset fixed;
    fixed.name = "Fixed places";
    fixed.prizes = defaultPrizes();
    presets.push_back(std::move(fixed));
    return presets;
}

const PrizePreset& TournamentConfig::prizePreset() const {
    static const PrizePreset fallback;
    if (prizePresets.empty()) return fallback;
    int index = activePrizePreset;
    if (index < 0 || index >= static_cast<int>(prizePresets.size())) index = 0;
    return prizePresets[static_cast<std::size_t>(index)];
}

PrizePreset& TournamentConfig::prizePreset() {
    if (prizePresets.empty()) prizePresets = defaultPrizePresets();
    if (activePrizePreset < 0 || activePrizePreset >= static_cast<int>(prizePresets.size())) {
        activePrizePreset = 0;
    }
    return prizePresets[static_cast<std::size_t>(activePrizePreset)];
}

int TournamentConfig::indexOfPlayingLevel(int number) const noexcept {
    if (number < 1) return -1;
    int found = 0;
    for (int i = 0; i < static_cast<int>(blinds.size()); ++i) {
        if (blinds[static_cast<std::size_t>(i)].isBreak) continue;
        ++found;
        if (found == number) return i;
    }
    return -1;
}

} // namespace pokersoc::domain
