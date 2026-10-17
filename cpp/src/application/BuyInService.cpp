#include "application/BuyInService.h"

#include <memory>
#include <utility>

#include "domain/cashbox/Cashbox.h"
#include "domain/player/Player.h"
#include "domain/transaction/BuyIn.h"
#include "domain/transaction/TransactionLog.h"
#include "persistence/AppStore.h"

namespace pokersoc::application {

using domain::BuyIn;
using domain::BuyInQuote;
using domain::Cashbox;
using domain::DenominationCount;
using domain::Money;
using domain::Rebuy;
using domain::TransactionType;

int BuyInService::existingBuyInCount(const std::string& playerId) const {
    int count = 0;
    for (const auto& transaction : log_.entries()) {
        const TransactionType type = transaction->type();
        if ((type == TransactionType::BuyIn || type == TransactionType::Rebuy) &&
            transaction->playerId() == playerId) {
            ++count;
        }
    }
    return count;
}

BuyInQuote BuyInService::preview(const domain::Player& player, Money requestedAmount) const {
    return mode_.quoteBuyIn(player, requestedAmount, existingBuyInCount(player.id()));
}

BuyInService::Result BuyInService::buyIn(const domain::Player& player,
                                         const DenominationCount& cashReceived,
                                         Money requestedAmount) {
    Result result;

    const BuyInQuote quote =
        mode_.quoteBuyIn(player, requestedAmount, existingBuyInCount(player.id()));
    if (!quote.allowed) {
        result.message = quote.message;
        return result;
    }

    const Money cashTotal = cashReceived.total();
    if (cashTotal < quote.amount) {
        result.message = "Not enough cash: buy-in is " + quote.amount.toString() +
                         " but only " + cashTotal.toString() + " was tendered.";
        return result;
    }

    const Money changeDue = cashTotal - quote.amount;
    DenominationCount change;
    if (!changeDue.isZero()) {
        // Change can be drawn from the float plus the cash just handed over, so
        // probe a copy of the cashbox that already holds the received cash.
        Cashbox probe = cashbox_;
        probe.applyCount(cashReceived, +1, domain::MovementReason::BuyIn);
        auto made = probe.makeChange(changeDue);
        if (!made) {
            result.message = "Cannot make change for " + changeDue.toString() +
                             " from the cashbox.";
            return result;
        }
        change = std::move(*made);
    }

    std::unique_ptr<BuyIn> transaction =
        quote.isRebuy ? std::make_unique<Rebuy>(player.id(), cashReceived, change,
                                                quote.amount, quote.label)
                      : std::make_unique<BuyIn>(player.id(), cashReceived, change,
                                                quote.amount, quote.label);

    store_.recordBuyIn(*transaction); // persist first (crash-safe)
    transaction->apply(cashbox_);      // then mutate the in-memory cashbox

    result.success = true;
    result.isRebuy = quote.isRebuy;
    result.buyInAmount = quote.amount;
    result.change = change;
    result.message = (quote.isRebuy ? "Rebuy " : "Buy-in ") + quote.amount.toString() + " for " +
                     (player.id().empty() ? "player" : player.id()) +
                     (changeDue.isZero() ? "" : (", change " + changeDue.toString())) + ".";
    result.transaction = transaction.get();
    log_.add(std::move(transaction));
    return result;
}

} // namespace pokersoc::application
