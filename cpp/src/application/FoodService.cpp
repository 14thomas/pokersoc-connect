#include "application/FoodService.h"

#include <memory>
#include <stdexcept>
#include <utility>

#include "application/CashOutService.h" // roundToFiveCents
#include "domain/cashbox/Cashbox.h"
#include "domain/food/ProductCatalog.h"
#include "domain/money/DenominationTable.h"
#include "domain/session/SessionMode.h"
#include "domain/transaction/FoodSale.h"
#include "persistence/AppStore.h"

namespace pokersoc::application {

using domain::DenominationCount;
using domain::FoodSale;
using domain::Money;
using domain::PaymentMethod;
using domain::SaleLine;

namespace {

/// Greedy breakdown over an unlimited supply (chips are not inventoried).
DenominationCount greedyBreakdown(Money amount,
                                  const std::vector<std::shared_ptr<const domain::Denomination>>& denoms) {
    DenominationCount count;
    std::int64_t remaining = amount.cents();
    for (const auto& denomination : denoms) {
        const std::int64_t face = denomination->cents();
        if (face <= 0 || remaining < face) continue;
        const auto qty = static_cast<int>(remaining / face);
        count.add(denomination, qty);
        remaining -= face * qty;
    }
    return count;
}

std::string validateLines(const std::vector<SaleLine>& lines) {
    if (lines.empty()) {
        return "Nothing selected.";
    }
    for (const auto& line : lines) {
        if (line.qty <= 0) return "Quantities must be positive.";
    }
    return "";
}

} // namespace

const domain::Product& FoodService::saveProduct(domain::Product product) {
    product.id = store_.saveProduct(product);
    return catalog_.upsert(std::move(product));
}

bool FoodService::removeProduct(long long id) {
    if (!catalog_.find(id)) {
        return false;
    }
    store_.deleteProduct(id);
    catalog_.remove(id);
    return true;
}

void FoodService::clearCatalog() {
    for (const auto* product : catalog_.all()) {
        store_.deleteProduct(product->id);
    }
    catalog_.clear();
}

void FoodService::setSlot(int slot, long long productId) {
    store_.saveFoodSlot(slot, productId);
    catalog_.setSlot(slot, productId);
}

bool FoodService::chipPaymentAllowed() const noexcept {
    return mode_.chipsHaveCashValue();
}

FoodService::SaleResult FoodService::sellForCash(const std::string& playerId,
                                                 const std::vector<SaleLine>& lines,
                                                 const DenominationCount& payment) {
    SaleResult result;
    if (const std::string problem = validateLines(lines); !problem.empty()) {
        result.message = problem;
        return result;
    }
    const Money total = domain::saleLinesTotal(lines);
    const Money paid = payment.total();
    if (paid < total) {
        result.message = "Not enough cash: total is " + total.toString() + ", paid " +
                         paid.toString() + ".";
        return result;
    }

    const Money changeDue = roundToFiveCents(paid - total);
    DenominationCount change;
    if (!changeDue.isZero()) {
        domain::Cashbox probe = ledger_.cashbox();
        probe.applyCount(payment, +1, domain::MovementReason::Sale);
        auto made = probe.makeChange(changeDue);
        if (!made) {
            result.message = "Cannot make change for " + changeDue.toString() + " from the cashbox.";
            return result;
        }
        change = std::move(*made);
    }

    try {
        result.transaction = &ledger_.commit(
            std::make_unique<FoodSale>(playerId, lines, PaymentMethod::Cash, payment, change));
    } catch (const std::logic_error& error) {
        result.message = error.what();
        return result;
    }
    result.success = true;
    result.change = change;
    result.message = "Sold " + domain::describeSaleLines(lines) + " for " + total.toString() +
                     (changeDue.isZero() ? "" : ", change " + changeDue.toString()) + ".";
    return result;
}

FoodService::SaleResult FoodService::sellForChips(const std::string& playerId,
                                                  const std::vector<SaleLine>& lines,
                                                  const DenominationCount& chips) {
    SaleResult result;
    if (!chipPaymentAllowed()) {
        result.message = "Chips can only pay for food in a cash game.";
        return result;
    }
    if (const std::string problem = validateLines(lines); !problem.empty()) {
        result.message = problem;
        return result;
    }
    const Money total = domain::saleLinesTotal(lines);
    const Money paid = chips.total();
    if (paid < total) {
        result.message = "Not enough chips: total is " + total.toString() + ", paid " +
                         paid.toString() + ".";
        return result;
    }

    const Money changeDue = roundToFiveCents(paid - total);
    const DenominationCount change = greedyBreakdown(changeDue, table_.chips());

    try {
        result.transaction = &ledger_.commit(
            std::make_unique<FoodSale>(playerId, lines, PaymentMethod::Chips, chips, change));
    } catch (const std::logic_error& error) {
        result.message = error.what();
        return result;
    }
    result.success = true;
    result.change = change;
    result.message = "Sold " + domain::describeSaleLines(lines) + " for " + total.toString() +
                     " in chips" +
                     (changeDue.isZero() ? "" : ", chip change " + changeDue.toString()) + ".";
    return result;
}

} // namespace pokersoc::application
