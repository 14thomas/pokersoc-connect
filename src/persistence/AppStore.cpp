#include "persistence/AppStore.h"

namespace pokersoc::persistence {

using domain::CashGameConfig;
using domain::Money;
using domain::SessionModeType;
using domain::TournamentConfig;

void AppStore::setIntSetting(const std::string& key, long long value) {
    setSetting(key, std::to_string(value));
}

long long AppStore::getIntSetting(const std::string& key, long long fallback) {
    const auto value = getSetting(key);
    if (!value) {
        return fallback;
    }
    try {
        return std::stoll(*value);
    } catch (...) {
        return fallback;
    }
}

void AppStore::saveSessionMode(SessionModeType type) {
    setSetting("session_mode", domain::sessionModeTypeName(type));
}

std::optional<SessionModeType> AppStore::loadSessionModeType() {
    const auto value = getSetting("session_mode");
    if (!value) {
        return std::nullopt;
    }
    return domain::sessionModeTypeFromName(*value);
}

void AppStore::saveCashGameConfig(const CashGameConfig& config) {
    setIntSetting("cash.min_cents", config.minBuyIn.cents());
    setIntSetting("cash.max_cents", config.maxBuyIn.cents());
    setIntSetting("cash.limit_cents", config.sessionLimit.cents());
}

CashGameConfig AppStore::loadCashGameConfig() {
    CashGameConfig config; // defaults
    config.minBuyIn = Money::fromCents(getIntSetting("cash.min_cents", config.minBuyIn.cents()));
    config.maxBuyIn = Money::fromCents(getIntSetting("cash.max_cents", config.maxBuyIn.cents()));
    config.sessionLimit =
        Money::fromCents(getIntSetting("cash.limit_cents", config.sessionLimit.cents()));
    return config;
}

void AppStore::saveTournamentConfig(const TournamentConfig& config) {
    setSetting("tnmt.pricing", domain::tournamentPricingName(config.pricing));
    setIntSetting("tnmt.arc_only", config.arcOnly ? 1 : 0);
    setIntSetting("tnmt.rebuy_cap", config.rebuyCap);

    setIntSetting("tnmt.arc_buyin", config.arcBuyInCents);
    setIntSetting("tnmt.nonarc_buyin", config.nonArcBuyInCents);
    setIntSetting("tnmt.arc_rebuy", config.arcRebuyCents);
    setIntSetting("tnmt.nonarc_rebuy", config.nonArcRebuyCents);

    setIntSetting("tnmt.bronze_buyin", config.bronzeBuyInCents);
    setIntSetting("tnmt.silver_buyin", config.silverBuyInCents);
    setIntSetting("tnmt.gold_buyin", config.goldBuyInCents);
    setIntSetting("tnmt.platinum_buyin", config.platinumBuyInCents);
    setIntSetting("tnmt.diamond_buyin", config.diamondBuyInCents);

    setIntSetting("tnmt.bronze_rebuy", config.bronzeRebuyCents);
    setIntSetting("tnmt.silver_rebuy", config.silverRebuyCents);
    setIntSetting("tnmt.gold_rebuy", config.goldRebuyCents);
    setIntSetting("tnmt.platinum_rebuy", config.platinumRebuyCents);
    setIntSetting("tnmt.diamond_rebuy", config.diamondRebuyCents);

    setSetting("tnmt.blinds", config.blindsJson());
    setSetting("tnmt.prize_presets", config.prizePresetsJson());
    setSetting("tnmt.board_chips", config.boardChipsJson());
    setSetting("tnmt.prize_rounding", domain::prizeRoundingName(config.prizeRounding));
    setIntSetting("tnmt.prize_leftover_first", config.leftoverToFirst ? 1 : 0);

    setSetting("tnmt.name", config.name);
    setIntSetting("tnmt.starting_stack", config.startingStack);
    setIntSetting("tnmt.rego_ends_level", config.registrationEndsLevel);
    setIntSetting("tnmt.board.rego", config.board.registrationEnd ? 1 : 0);
    setIntSetting("tnmt.board.level", config.board.level ? 1 : 0);
    setIntSetting("tnmt.board.break", config.board.timeToBreak ? 1 : 0);
    setIntSetting("tnmt.board.break_len", config.board.breakLength ? 1 : 0);
    setIntSetting("tnmt.board.avg", config.board.avgStack ? 1 : 0);
    setIntSetting("tnmt.board.players", config.board.players ? 1 : 0);
    setIntSetting("tnmt.board.reentries", config.board.reentries ? 1 : 0);
    setIntSetting("tnmt.board.total", config.board.total ? 1 : 0);
    setIntSetting("tnmt.board.remaining", config.board.remaining ? 1 : 0);

    const auto saveFees = [this](const char* bountyKey, const char* rakeKey, const domain::BuyInFees& fees) {
        setIntSetting(bountyKey, fees.bountyCents);
        setIntSetting(rakeKey, fees.rakeCents);
    };
    saveFees("tnmt.arc_bounty", "tnmt.arc_rake", config.arcFees);
    saveFees("tnmt.nonarc_bounty", "tnmt.nonarc_rake", config.nonArcFees);
    saveFees("tnmt.arc_rebuy_bounty", "tnmt.arc_rebuy_rake", config.arcRebuyFees);
    saveFees("tnmt.nonarc_rebuy_bounty", "tnmt.nonarc_rebuy_rake", config.nonArcRebuyFees);
    saveFees("tnmt.bronze_bounty", "tnmt.bronze_rake", config.bronzeFees);
    saveFees("tnmt.silver_bounty", "tnmt.silver_rake", config.silverFees);
    saveFees("tnmt.gold_bounty", "tnmt.gold_rake", config.goldFees);
    saveFees("tnmt.platinum_bounty", "tnmt.platinum_rake", config.platinumFees);
    saveFees("tnmt.diamond_bounty", "tnmt.diamond_rake", config.diamondFees);
    saveFees("tnmt.bronze_rebuy_bounty", "tnmt.bronze_rebuy_rake", config.bronzeRebuyFees);
    saveFees("tnmt.silver_rebuy_bounty", "tnmt.silver_rebuy_rake", config.silverRebuyFees);
    saveFees("tnmt.gold_rebuy_bounty", "tnmt.gold_rebuy_rake", config.goldRebuyFees);
    saveFees("tnmt.platinum_rebuy_bounty", "tnmt.platinum_rebuy_rake", config.platinumRebuyFees);
    saveFees("tnmt.diamond_rebuy_bounty", "tnmt.diamond_rebuy_rake", config.diamondRebuyFees);
}

TournamentConfig AppStore::loadTournamentConfig() {
    TournamentConfig config; // defaults
    if (const auto pricing = getSetting("tnmt.pricing")) {
        config.pricing = domain::tournamentPricingFromName(*pricing);
    }
    config.arcOnly = getIntSetting("tnmt.arc_only", config.arcOnly ? 1 : 0) != 0;
    config.rebuyCap = static_cast<int>(getIntSetting("tnmt.rebuy_cap", config.rebuyCap));

    config.arcBuyInCents = getIntSetting("tnmt.arc_buyin", config.arcBuyInCents);
    config.nonArcBuyInCents = getIntSetting("tnmt.nonarc_buyin", config.nonArcBuyInCents);
    config.arcRebuyCents = getIntSetting("tnmt.arc_rebuy", config.arcRebuyCents);
    config.nonArcRebuyCents = getIntSetting("tnmt.nonarc_rebuy", config.nonArcRebuyCents);

    config.bronzeBuyInCents = getIntSetting("tnmt.bronze_buyin", config.bronzeBuyInCents);
    config.silverBuyInCents = getIntSetting("tnmt.silver_buyin", config.silverBuyInCents);
    config.goldBuyInCents = getIntSetting("tnmt.gold_buyin", config.goldBuyInCents);
    config.platinumBuyInCents = getIntSetting("tnmt.platinum_buyin", config.platinumBuyInCents);
    config.diamondBuyInCents = getIntSetting("tnmt.diamond_buyin", config.diamondBuyInCents);

    config.bronzeRebuyCents = getIntSetting("tnmt.bronze_rebuy", config.bronzeRebuyCents);
    config.silverRebuyCents = getIntSetting("tnmt.silver_rebuy", config.silverRebuyCents);
    config.goldRebuyCents = getIntSetting("tnmt.gold_rebuy", config.goldRebuyCents);
    config.platinumRebuyCents = getIntSetting("tnmt.platinum_rebuy", config.platinumRebuyCents);
    config.diamondRebuyCents = getIntSetting("tnmt.diamond_rebuy", config.diamondRebuyCents);

    if (const auto blinds = getSetting("tnmt.blinds")) config.setBlindsFromJson(*blinds);
    if (const auto presets = getSetting("tnmt.prize_presets")) {
        config.setPrizePresetsFromJson(*presets);
    } else if (const auto prizes = getSetting("tnmt.prizes")) {
        config.setPrizesFromJson(*prizes);
    }
    if (const auto chips = getSetting("tnmt.board_chips")) config.setBoardChipsFromJson(*chips);
    if (const auto rounding = getSetting("tnmt.prize_rounding")) {
        config.prizeRounding = domain::prizeRoundingFromName(*rounding);
    }
    config.leftoverToFirst =
        getIntSetting("tnmt.prize_leftover_first", config.leftoverToFirst ? 1 : 0) != 0;

    if (const auto name = getSetting("tnmt.name")) {
        if (!name->empty()) config.name = *name;
    }
    config.startingStack =
        static_cast<int>(getIntSetting("tnmt.starting_stack", config.startingStack));
    if (config.startingStack < 1) config.startingStack = 1;
    config.registrationEndsLevel =
        static_cast<int>(getIntSetting("tnmt.rego_ends_level", config.registrationEndsLevel));
    config.board.registrationEnd = getIntSetting("tnmt.board.rego", 1) != 0;
    config.board.level = getIntSetting("tnmt.board.level", 1) != 0;
    config.board.timeToBreak = getIntSetting("tnmt.board.break", 1) != 0;
    config.board.breakLength = getIntSetting("tnmt.board.break_len", 1) != 0;
    config.board.avgStack = getIntSetting("tnmt.board.avg", 1) != 0;
    config.board.players = getIntSetting("tnmt.board.players", 1) != 0;
    config.board.reentries = getIntSetting("tnmt.board.reentries", 1) != 0;
    config.board.total = getIntSetting("tnmt.board.total", 1) != 0;
    config.board.remaining = getIntSetting("tnmt.board.remaining", 1) != 0;

    const long long legacyBounty = getIntSetting("tnmt.bounty_cents", 0);
    const long long legacyRake = getIntSetting("tnmt.rake_cents", 0);
    const auto loadFees = [&](const char* bountyKey, const char* rakeKey, domain::BuyInFees& fees) {
        fees.bountyCents = getIntSetting(bountyKey, legacyBounty);
        fees.rakeCents = getIntSetting(rakeKey, legacyRake);
    };
    loadFees("tnmt.arc_bounty", "tnmt.arc_rake", config.arcFees);
    loadFees("tnmt.nonarc_bounty", "tnmt.nonarc_rake", config.nonArcFees);
    loadFees("tnmt.arc_rebuy_bounty", "tnmt.arc_rebuy_rake", config.arcRebuyFees);
    loadFees("tnmt.nonarc_rebuy_bounty", "tnmt.nonarc_rebuy_rake", config.nonArcRebuyFees);
    loadFees("tnmt.bronze_bounty", "tnmt.bronze_rake", config.bronzeFees);
    loadFees("tnmt.silver_bounty", "tnmt.silver_rake", config.silverFees);
    loadFees("tnmt.gold_bounty", "tnmt.gold_rake", config.goldFees);
    loadFees("tnmt.platinum_bounty", "tnmt.platinum_rake", config.platinumFees);
    loadFees("tnmt.diamond_bounty", "tnmt.diamond_rake", config.diamondFees);
    loadFees("tnmt.bronze_rebuy_bounty", "tnmt.bronze_rebuy_rake", config.bronzeRebuyFees);
    loadFees("tnmt.silver_rebuy_bounty", "tnmt.silver_rebuy_rake", config.silverRebuyFees);
    loadFees("tnmt.gold_rebuy_bounty", "tnmt.gold_rebuy_rake", config.goldRebuyFees);
    loadFees("tnmt.platinum_rebuy_bounty", "tnmt.platinum_rebuy_rake", config.platinumRebuyFees);
    loadFees("tnmt.diamond_rebuy_bounty", "tnmt.diamond_rebuy_rake", config.diamondRebuyFees);
    return config;
}

} // namespace pokersoc::persistence
