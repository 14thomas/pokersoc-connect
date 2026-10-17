#include "domain/session/TournamentMode.h"

#include <string>

#include "domain/player/Player.h"

namespace pokersoc::domain {

BuyInQuote TournamentMode::quoteBuyIn(const Player& player,
                                      Money /*requested*/,
                                      int existingBuyInCount) const {
    const bool isRebuy = existingBuyInCount > 0;

    if (!config_.eligible(player)) {
        return BuyInQuote::deny(
            "This is an ARC-only tournament; " +
                (player.id().empty() ? std::string("the player") : player.id()) +
                " is not an ARC member.",
            isRebuy);
    }

    if (!config_.canBuyIn(existingBuyInCount)) {
        const std::string cap =
            config_.rebuyCap <= 0 ? "unlimited" : std::to_string(config_.rebuyCap);
        return BuyInQuote::deny("Rebuy limit reached (cap " + cap + "); no further buy-ins.",
                                isRebuy);
    }

    BuyInQuote quote;
    quote.allowed = true;
    quote.isRebuy = isRebuy;
    quote.amount = config_.priceFor(player, isRebuy);
    quote.label = config_.labelFor(player, isRebuy);
    return quote;
}

std::string TournamentMode::rulesSummary() const {
    const std::string cap =
        config_.rebuyCap <= 0 ? "unlimited rebuys" : (std::to_string(config_.rebuyCap) + " rebuy(s) max");
    return "Tournament: fixed buy-ins priced by " + config_.pricingSummary() + "; " + cap +
           ". Payouts are prize-based (not yet configured).";
}

} // namespace pokersoc::domain
