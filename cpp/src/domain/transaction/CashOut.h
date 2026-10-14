#pragma once

#include <string>

#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"
#include "domain/transaction/Transaction.h"

namespace pokersoc::domain {

class Cashbox;

/// A player hands chips back and receives cash. Because chips are valued 1:1
/// with cash, the chip value equals the cash the player is owed; an optional
/// tip (e.g. lost / donated chips) is kept by the house and reduces the payout.
///
///   cashPaidOut = chipsReturned.total() - tip
class CashOut final : public Transaction {
public:
    CashOut(std::string playerId,
            DenominationCount chipsReturned,
            DenominationCount cashPaidOut,
            Money tip = Money{});

    TransactionType type() const noexcept override { return TransactionType::CashOut; }
    Money amount() const noexcept override { return cashPaidOut_.total(); }

    void apply(Cashbox& cashbox) const override;   // withdraw the cash paid out
    void reverse(Cashbox& cashbox) const override; // put it back

    std::string describe() const override;

    const DenominationCount& chipsReturned() const noexcept { return chipsReturned_; }
    const DenominationCount& cashPaidOut() const noexcept { return cashPaidOut_; }
    Money chipValue() const noexcept { return chipsReturned_.total(); }
    Money tip() const noexcept { return tip_; }

private:
    DenominationCount chipsReturned_;
    DenominationCount cashPaidOut_;
    Money tip_;
};

} // namespace pokersoc::domain
