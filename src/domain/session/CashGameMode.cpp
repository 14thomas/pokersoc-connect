#include "domain/session/CashGameMode.h"

namespace pokersoc::domain {

BuyInQuote CashGameMode::quoteBuyIn(const Player& /*player*/,
                                    Money requested,
                                    const BuyInHistory& history) const {
    const bool isRebuy = history.count > 0;
    const auto maxThis = config_.ceiling(history);
    Money minThis = config_.minBuyIn;
    if (maxThis && minThis > *maxThis) {
        minThis = *maxThis; // leftover under the cap is smaller than the usual min
    }

    if (requested < minThis) {
        return BuyInQuote::deny("Buy-in " + requested.toString() +
                                    " is below the minimum " + minThis.toString() + ".",
                                isRebuy);
    }
    if (maxThis && requested > *maxThis) {
        // Name the bound that actually sets the ceiling. A $15 per-buy-in max
        // beats a $45 daily cap; $5 left on the daily cap beats a $15 max.
        const bool perBuyInTighter =
            config_.hasMaxBuyIn() &&
            (!config_.hasSessionCap() || config_.maxBuyIn <= config_.remaining(history));
        if (perBuyInTighter) {
            return BuyInQuote::deny("The per buy-in limit is " + config_.maxBuyIn.toString() + ".",
                                    isRebuy);
        }
        return BuyInQuote::deny("The daily limit is " + config_.remaining(history).toString() +
                                    " remaining.",
                                isRebuy);
    }

    BuyInQuote quote;
    quote.allowed = true;
    quote.amount = requested;
    quote.isRebuy = isRebuy;
    quote.label = isRebuy ? "Cash game rebuy" : "Cash game buy-in";
    return quote;
}

std::string CashGameMode::rulesSummary() const {
    std::string text = "Cash game: buy in for ";
    if (config_.hasMaxBuyIn()) {
        text += "any amount between " + config_.minBuyIn.toString() + " and " +
                config_.maxBuyIn.toString();
    } else if (config_.minBuyIn > Money{}) {
        text += "any amount of at least " + config_.minBuyIn.toString();
    } else {
        text += "any amount";
    }
    text += "; cash chips out 1:1 any time.";
    if (config_.hasSessionCap()) {
        text += " Session cap " + config_.sessionLimit.toString() + " per player.";
    }
    return text;
}

} // namespace pokersoc::domain
