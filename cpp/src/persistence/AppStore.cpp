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
    return config;
}

} // namespace pokersoc::persistence
