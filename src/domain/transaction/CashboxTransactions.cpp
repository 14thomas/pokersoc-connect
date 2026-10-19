#include "domain/transaction/CashboxTransactions.h"

#include <utility>

#include "domain/cashbox/Cashbox.h"

namespace pokersoc::domain {

std::string describeCount(const DenominationCount& count) {
    std::string text;
    for (const auto& entry : count.entries()) {
        if (!text.empty()) text += ", ";
        text += std::to_string(entry.qty) + " x " + entry.denomination->label();
    }
    return text.empty() ? "(none)" : text;
}

// ---- FloatAdd ----

FloatAdd::FloatAdd(DenominationCount cash) : Transaction(""), cash_(std::move(cash)) {}

void FloatAdd::apply(Cashbox& cashbox) const {
    cashbox.applyCount(cash_, +1, MovementReason::FloatAdd, "", "Float");
}

void FloatAdd::reverse(Cashbox& cashbox) const {
    cashbox.applyCount(cash_, -1, MovementReason::Undo, "", "Undo float");
}

std::string FloatAdd::describe() const {
    return "Float added " + amount().toString() + ": " + describeCount(cash_);
}

// ---- CashTip ----

CashTip::CashTip(DenominationCount cash, std::string playerId)
    : Transaction(std::move(playerId)), cash_(std::move(cash)) {}

void CashTip::apply(Cashbox& cashbox) const {
    cashbox.applyCount(cash_, +1, MovementReason::Tip, playerId_, "Cash tip");
}

void CashTip::reverse(Cashbox& cashbox) const {
    cashbox.applyCount(cash_, -1, MovementReason::Undo, playerId_, "Undo cash tip");
}

std::string CashTip::describe() const {
    std::string text = "Cash tip " + amount().toString() + ": " + describeCount(cash_);
    if (!notes_.empty()) text += " - " + notes_;
    return text;
}

// ---- LostChips ----

LostChips::LostChips(DenominationCount chips) : Transaction(""), chips_(std::move(chips)) {}

std::string LostChips::describe() const {
    std::string text = "Lost chips kept as tips " + amount().toString() + ": " + describeCount(chips_);
    if (!notes_.empty()) text += " - " + notes_;
    return text;
}

// ---- Exchange ----

Exchange::Exchange(DenominationCount given, DenominationCount received)
    : Transaction(""), given_(std::move(given)), received_(std::move(received)) {}

void Exchange::apply(Cashbox& cashbox) const {
    cashbox.applyCount(received_, +1, MovementReason::Exchange, "", "Exchange in");
    cashbox.applyCount(given_, -1, MovementReason::Exchange, "", "Exchange out");
}

void Exchange::reverse(Cashbox& cashbox) const {
    cashbox.applyCount(given_, +1, MovementReason::Undo, "", "Undo exchange out");
    cashbox.applyCount(received_, -1, MovementReason::Undo, "", "Undo exchange in");
}

std::string Exchange::describe() const {
    return "Exchange " + amount().toString() + ": gave [" + describeCount(given_) +
           "] for [" + describeCount(received_) + "]";
}

} // namespace pokersoc::domain
