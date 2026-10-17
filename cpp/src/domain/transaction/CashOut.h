#pragma once

#include <string>
#include <vector>

#include "domain/food/SaleLine.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"
#include "domain/transaction/Transaction.h"

namespace pokersoc::domain {

class Cashbox;

/// A player hands chips back and receives cash. Chips are valued 1:1 with cash.
/// At the same time the player may buy food (paid out of the chips), leave a
/// tip, or add extra cash (e.g. to get a larger note back). The books balance as:
///
///   chips + extraCash  ==  cashPaidOut + food + tip   (give or take 5c rounding)
class CashOut final : public Transaction {
public:
    CashOut(std::string playerId,
            DenominationCount chipsReturned,
            DenominationCount cashPaidOut,
            Money tip = Money{},
            DenominationCount extraCash = DenominationCount{},
            std::vector<SaleLine> food = {});

    TransactionType type() const noexcept override { return TransactionType::CashOut; }
    Money amount() const noexcept override { return cashPaidOut_.total(); }

    void apply(Cashbox& cashbox) const override;   // extra cash in, payout out
    void reverse(Cashbox& cashbox) const override; // put it back

    std::string describe() const override;

    const DenominationCount& chipsReturned() const noexcept { return chipsReturned_; }
    const DenominationCount& cashPaidOut() const noexcept { return cashPaidOut_; }
    const DenominationCount& extraCash() const noexcept { return extraCash_; }
    const std::vector<SaleLine>& food() const noexcept { return food_; }

    Money chipValue() const noexcept { return chipsReturned_.total(); }
    Money tip() const noexcept { return tip_; }
    Money foodTotal() const noexcept { return saleLinesTotal(food_); }

private:
    DenominationCount chipsReturned_;
    DenominationCount cashPaidOut_;
    Money tip_;
    DenominationCount extraCash_;
    std::vector<SaleLine> food_;
};

} // namespace pokersoc::domain
