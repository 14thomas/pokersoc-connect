#include "domain/session/CashGameMode.h"

namespace pokersoc::domain {

BuyInQuote CashGameMode::quoteBuyIn(const Player& /*player*/,
                                    Money requested,
                                    int existingBuyInCount) const {
    const bool isRebuy = existingBuyInCount > 0;

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
    return quote;
}

std::string CashGameMode::rulesSummary() const {
    return "Cash game: buy in for any amount between " + config_.minBuyIn.toString() +
           " and " + config_.maxBuyIn.toString() + "; cash chips out 1:1 any time.";
}

} // namespace pokersoc::domain
