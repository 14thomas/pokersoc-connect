#include "domain/session/CashGameMode.h"

namespace pokersoc::domain {

BuyInQuote CashGameMode::quoteBuyIn(const Player& /*player*/,
                                    Money requested,
                                    const BuyInHistory& history) const {
    const bool isRebuy = history.count > 0;

    if (requested < config_.minBuyIn) {
        return BuyInQuote::deny("Buy-in " + requested.toString() +
                                    " is below the minimum " + config_.minBuyIn.toString() + ".",
                                isRebuy);
    }
    if (requested > config_.maxBuyIn) {
        return BuyInQuote::deny("Buy-in " + requested.toString() +
                                    " is above the maximum " + config_.maxBuyIn.toString() + ".",
                                isRebuy);
    }

    BuyInQuote quote;
    quote.allowed = true;
    quote.amount = requested;
    quote.isRebuy = isRebuy;
    quote.label = isRebuy ? "Cash game rebuy" : "Cash game buy-in";

    const Money newTotal = history.total + requested;
    if (config_.sessionLimit > Money{} && newTotal > config_.sessionLimit) {
        quote.needsConfirmation = true;
        quote.confirmation = "Session buy-ins so far: " + history.total.toString() +
                             "\nThis buy-in: " + requested.toString() +
                             "\nNew total: " + newTotal.toString() + " (over the " +
                             config_.sessionLimit.toString() +
                             " limit)\nHas the player taken their timeout?";
    }
    return quote;
}

std::string CashGameMode::rulesSummary() const {
    std::string text = "Cash game: buy in for any amount between " + config_.minBuyIn.toString() +
                       " and " + config_.maxBuyIn.toString() + "; cash chips out 1:1 any time.";
    if (config_.sessionLimit > Money{}) {
        text += " Going over " + config_.sessionLimit.toString() +
                " in a session requires a timeout.";
    }
    return text;
}

} // namespace pokersoc::domain
