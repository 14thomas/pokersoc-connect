#include "application/BuyInService.h"

#include <memory>
#include <stdexcept>
#include <utility>

#include "application/Ledger.h"
#include "domain/cashbox/Cashbox.h"
#include "domain/player/Player.h"
#include "domain/transaction/BuyIn.h"
#include "domain/transaction/TransactionLog.h"

namespace pokersoc::application {

using domain::BuyIn;
using domain::BuyInHistory;
using domain::BuyInQuote;
using domain::Cashbox;
using domain::DenominationCount;
using domain::Money;
using domain::Rebuy;
using domain::TransactionType;

BuyInHistory BuyInService::history(const std::string& playerId) const {
    BuyInHistory history;
    for (const auto& transaction : ledger_.log().entries()) {
        const TransactionType type = transaction->type();
        if ((type == TransactionType::BuyIn || type == TransactionType::Rebuy) &&
            transaction->playerId() == playerId) {
            ++history.count;
            history.total += transaction->amount();
        }
    }
    return history;
}

BuyInQuote BuyInService::preview(const domain::Player& player, Money requestedAmount) const {
    if (player.isUnderage()) {
        return BuyInQuote::deny("Player is under 18 and cannot buy in.");
    }
    return mode_.quoteBuyIn(player, requestedAmount, history(player.id()));
}

BuyInService::Result BuyInService::buyIn(const domain::Player& player,
                                         const DenominationCount& cashReceived,
                                         Money requestedAmount,
                                         const std::string& notes) {
    Result result;

    const BuyInQuote quote = preview(player, requestedAmount);
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
        Cashbox probe = ledger_.cashbox();
        probe.applyCount(cashReceived, +1, domain::MovementReason::BuyIn);
        auto made = probe.makeChange(changeDue);
        if (!made) {
            result.message = "Cannot make change for " + changeDue.toString() +
                             " from the cashbox.";
            return result;
        }
        change = std::move(*made);
    }

    std::string label = quote.label;
    if (!notes.empty()) {
        label += " - " + notes;
    }

    Money prize = quote.prize;
    Money bounty = quote.bounty;
    Money rake = quote.rake;
    if (prize.isZero() && bounty.isZero() && rake.isZero()) prize = quote.amount;

    std::unique_ptr<BuyIn> transaction =
        quote.isRebuy
            ? std::make_unique<Rebuy>(player.id(), cashReceived, change, quote.amount, label, prize,
                                      bounty, rake)
            : std::make_unique<BuyIn>(player.id(), cashReceived, change, quote.amount, label, prize,
                                      bounty, rake);

    try {
        result.transaction = &ledger_.commit(std::move(transaction));
    } catch (const std::logic_error& error) {
        result.message = error.what();
        return result;
    }

    result.success = true;
    result.isRebuy = quote.isRebuy;
    result.buyInAmount = quote.amount;
    result.change = change;
    result.message = (quote.isRebuy ? "Rebuy " : "Buy-in ") + quote.amount.toString() + " for " +
                     (player.name().empty() ? player.id() : player.name()) +
                     (changeDue.isZero() ? "" : (", change " + changeDue.toString())) + ".";
    return result;
}

} // namespace pokersoc::application
