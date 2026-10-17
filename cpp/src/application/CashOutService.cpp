#include "application/CashOutService.h"

#include <memory>
#include <stdexcept>
#include <utility>

#include "application/Ledger.h"
#include "domain/cashbox/Cashbox.h"
#include "domain/player/Player.h"
#include "domain/session/SessionMode.h"
#include "domain/transaction/CashOut.h"

namespace pokersoc::application {

using domain::Cashbox;
using domain::CashOut;
using domain::DenominationCount;
using domain::Money;

namespace {

constexpr std::int64_t kCustomChangeToleranceCents = 2;

} // namespace

Money roundToFiveCents(Money amount) noexcept {
    const std::int64_t cents = amount.cents();
    if (cents < 0) {
        return -roundToFiveCents(-amount);
    }
    return Money::fromCents(((cents + 2) / 5) * 5);
}

CashOutService::Plan CashOutService::plan(const domain::Player* player,
                                          const Request& request) const {
    Plan plan;
    plan.chipValue = request.chips.total();
    plan.foodTotal = domain::saleLinesTotal(request.food);
    plan.tip = request.tip;
    plan.extraCash = request.extraCash.total();

    if (!mode_.allowsChipCashOut()) {
        plan.message = "Tournament cash-out is coming soon (prizing is not set up yet).";
        return plan;
    }
    if (player && player->isUnderage()) {
        plan.message = "Player is under 18 and cannot cash out.";
        return plan;
    }
    if (request.tip.isNegative()) {
        plan.message = "Tip cannot be negative.";
        return plan;
    }
    if (request.chips.empty() && request.extraCash.empty()) {
        plan.message = "No chips were handed in.";
        return plan;
    }

    const Money raw = plan.chipValue - plan.foodTotal - plan.tip + plan.extraCash;
    if (raw.isNegative()) {
        plan.message = "Food and tip exceed the chips by " + (-raw).toString() +
                       "; the player needs to add extra cash.";
        return plan;
    }
    plan.payout = roundToFiveCents(raw);

    // Change can come from the float plus any extra cash just handed over.
    Cashbox probe = ledger_.cashbox();
    probe.applyCount(request.extraCash, +1, domain::MovementReason::ExtraCash);

    if (request.customChange) {
        const Money given = request.customChange->total();
        if ((given - raw).abs().cents() > kCustomChangeToleranceCents) {
            plan.message = "Custom change totals " + given.toString() + " but " +
                           plan.payout.toString() + " is owed.";
            return plan;
        }
        if (!probe.canSupply(*request.customChange)) {
            plan.message = "The cashbox does not hold that change.";
            return plan;
        }
        plan.payout = given;
        plan.change = *request.customChange;
    } else if (!plan.payout.isZero()) {
        auto made = probe.makeChange(plan.payout);
        if (!made) {
            plan.message = "Cannot make change for " + plan.payout.toString() +
                           " from the cashbox.";
            return plan;
        }
        plan.change = std::move(*made);
    }

    plan.ok = true;
    return plan;
}

CashOutService::Result CashOutService::cashOut(const domain::Player* player,
                                               const Request& request) {
    Result result;
    const Plan worked = plan(player, request);
    if (!worked.ok) {
        result.message = worked.message;
        return result;
    }

    auto transaction = std::make_unique<CashOut>(request.playerId, request.chips, worked.change,
                                                 request.tip, request.extraCash, request.food);
    try {
        result.transaction = &ledger_.commit(std::move(transaction));
    } catch (const std::logic_error& error) {
        result.message = error.what();
        return result;
    }

    result.success = true;
    result.change = worked.change;
    result.message = "Cashed out " + worked.chipValue.toString() + " in chips; paid " +
                     worked.payout.toString() + ".";
    return result;
}

} // namespace pokersoc::application
