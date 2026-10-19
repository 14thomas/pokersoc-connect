#pragma once

#include <string>

#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"
#include "domain/transaction/Transaction.h"

namespace pokersoc::domain {

class Cashbox;

/// A player hands in cash and receives chips worth `buyInAmount`. Any cash
/// tendered above the buy-in is returned as `changeGiven`:
///
///   chips received  = buyInAmount
///   cash into house = cashReceived.total() - changeGiven.total()  (== buyInAmount)
///
/// Works for both session modes — the mode only decides the amount; the money
/// movement is identical. The pricing label (e.g. "ARC Member Rebuy") is kept in
/// the transaction notes so tournament stats survive a reload.
class BuyIn : public Transaction {
public:
    BuyIn(std::string playerId,
          DenominationCount cashReceived,
          DenominationCount changeGiven,
          Money buyInAmount,
          std::string label = "",
          Money prize = {},
          Money bounty = {},
          Money rake = {});

    TransactionType type() const noexcept override { return TransactionType::BuyIn; }
    Money amount() const noexcept override { return buyInAmount_; }

    void apply(Cashbox& cashbox) const override;   // cash in, change out
    void reverse(Cashbox& cashbox) const override; // put it back

    std::string describe() const override;

    const DenominationCount& cashReceived() const noexcept { return cashReceived_; }
    const DenominationCount& changeGiven() const noexcept { return changeGiven_; }
    Money buyInAmount() const noexcept { return buyInAmount_; }
    const std::string& label() const noexcept { return notes_; }
    virtual bool isRebuy() const noexcept { return false; }

    Money prizeAmount() const noexcept { return prize_; }
    Money bountyAmount() const noexcept { return bounty_; }
    Money rakeAmount() const noexcept { return rake_; }
    void setSplit(Money prize, Money bounty, Money rake);

protected:
    DenominationCount cashReceived_;
    DenominationCount changeGiven_;
    Money buyInAmount_;
    Money prize_;
    Money bounty_;
    Money rake_;
};

/// A rebuy is a buy-in made after the first; it counts toward the rebuy cap.
/// Same money mechanics as BuyIn, just tagged so reporting can tell them apart.
class Rebuy final : public BuyIn {
public:
    using BuyIn::BuyIn;

    TransactionType type() const noexcept override { return TransactionType::Rebuy; }
    bool isRebuy() const noexcept override { return true; }
};

} // namespace pokersoc::domain
