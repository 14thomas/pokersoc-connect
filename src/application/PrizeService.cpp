#include "application/PrizeService.h"

#include <memory>
#include <stdexcept>
#include <utility>

#include "application/CashOutService.h"
#include "application/Ledger.h"
#include "domain/cashbox/Cashbox.h"

namespace pokersoc::application {

using domain::DenominationCount;
using domain::Money;
using domain::TournamentPayout;

PrizeService::Result PrizeService::payPlacement(const std::string& playerId, int place,
                                                Money amount) {
    return pay(playerId, TournamentPayout::Kind::Placement, amount, place, 0);
}

PrizeService::Result PrizeService::payBounty(const std::string& playerId, int count,
                                             Money perBounty) {
    if (count < 1) {
        Result result;
        result.message = "Redeem at least one bounty.";
        return result;
    }
    return pay(playerId, TournamentPayout::Kind::Bounty, perBounty * count, 0, count);
}

PrizeService::Result PrizeService::pay(const std::string& playerId, TournamentPayout::Kind kind,
                                       Money amount, int place, int bountyCount) {
    Result result;
    if (playerId.empty()) {
        result.message = "Scan a player before paying out.";
        return result;
    }
    amount = roundToFiveCents(amount);
    if (amount.isZero() || amount.isNegative()) {
        result.message = "Payout amount must be greater than zero.";
        return result;
    }

    auto made = ledger_.cashbox().makeChange(amount);
    if (!made) {
        result.message = "Cannot make " + amount.toString() + " from the cashbox.";
        return result;
    }
    DenominationCount cash = std::move(*made);

    auto payout =
        std::make_unique<TournamentPayout>(playerId, kind, cash, place, bountyCount);
    try {
        result.transaction = &ledger_.commit(std::move(payout));
    } catch (const std::logic_error& error) {
        result.message = error.what();
        return result;
    }

    result.success = true;
    result.cash = cash;
    result.message = result.transaction->describe() + " for " + playerId + ".";
    return result;
}

} // namespace pokersoc::application
