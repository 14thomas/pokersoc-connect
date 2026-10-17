#include "domain/session/CashGameMode.h"

namespace pokersoc::domain {

BuyInQuote CashGameMode::quoteBuyIn(const Player& /*player*/,
                                    Money requested,
                                    const BuyInHistory& history) const {
    const bool isRebuy = history.count > 0;
    const bool capped = config_.hasSessionCap();
    const Money room = config_.remaining(history);

    if (capped && !config_.hasRoom(history)) {
        return BuyInQuote::deny("Player has reached the session buy-in cap of " +
                                    config_.sessionLimit.toString() + ".",
                                isRebuy);
    }

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
        if (capped && requested > room) {
            return BuyInQuote::deny("Buy-in " + requested.toString() +
                                        " would exceed the session cap of " +
                                        config_.sessionLimit.toString() + ". Remaining: " +
                                        room.toString() + ".",
                                    isRebuy);
        }
        return BuyInQuote::deny("Buy-in " + requested.toString() +
                                    " is above the maximum " + config_.maxBuyIn.toString() + ".",
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
