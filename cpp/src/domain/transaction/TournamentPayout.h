#pragma once

#include <string>

#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"
#include "domain/transaction/Transaction.h"

namespace pokersoc::domain {

class Cashbox;

/// Cash paid to a player for a finishing place or redeemed bounties.
class TournamentPayout final : public Transaction {
public:
    enum class Kind { Placement, Bounty };

    TournamentPayout(std::string playerId, Kind kind, DenominationCount cashPaid, int place = 0,
                     int bountyCount = 0);

    TransactionType type() const noexcept override;
    Money amount() const noexcept override { return cashPaid_.total(); }

    void apply(Cashbox& cashbox) const override;
    void reverse(Cashbox& cashbox) const override;
    std::string describe() const override;

    Kind kind() const noexcept { return kind_; }
    int place() const noexcept { return place_; }
    int bountyCount() const noexcept { return bountyCount_; }
    const DenominationCount& cashPaid() const noexcept { return cashPaid_; }

private:
    Kind kind_;
    DenominationCount cashPaid_;
    int place_ = 0;
    int bountyCount_ = 0;
};

} // namespace pokersoc::domain
