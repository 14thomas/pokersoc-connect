#include "domain/session/TournamentMode.h"

#include <string>

#include "domain/player/Player.h"

namespace pokersoc::domain {

BuyInQuote TournamentMode::quoteBuyIn(const Player& player,
                                      Money /*requested*/,
                                      const BuyInHistory& history) const {
    const bool isRebuy = history.count > 0;

    if (!config_.eligible(player)) {
        return BuyInQuote::deny(
            "This is an ARC-only tournament; " +
                (player.id().empty() ? std::string("the player") : player.id()) +
                " is not an ARC member.",
            isRebuy);
    }

    if (config_.usesMembershipPricing() && player.membership() == MembershipType::None) {
        return BuyInQuote::deny(
            "Membership pricing is in use; set the player's membership tier before buying in.",
            isRebuy);
    }

    if (!config_.canBuyIn(history.count)) {
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
    const BuyInSplit parts = config_.split(player, isRebuy);
    quote.prize = parts.prize;
    quote.bounty = parts.bounty;
    quote.rake = parts.rake;
    return quote;
}

std::string TournamentMode::rulesSummary() const {
    const std::string cap =
        config_.rebuyCap <= 0 ? "unlimited rebuys" : (std::to_string(config_.rebuyCap) + " rebuy(s) max");
    return "Tournament: fixed buy-ins priced by " + config_.pricingSummary() + "; " + cap +
           ". Payouts are prize and bounty based.";
}

} // namespace pokersoc::domain
