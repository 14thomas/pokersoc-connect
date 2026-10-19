#include "domain/transaction/TournamentPayout.h"

#include <utility>

#include "domain/cashbox/Cashbox.h"

namespace pokersoc::domain {

TournamentPayout::TournamentPayout(std::string playerId, Kind kind, DenominationCount cashPaid,
                                   int place, int bountyCount)
    : Transaction(std::move(playerId)),
      kind_(kind),
      cashPaid_(std::move(cashPaid)),
      place_(place),
      bountyCount_(bountyCount) {
    if (kind_ == Kind::Placement) {
        notes_ = "Place " + std::to_string(place_);
    } else {
        notes_ = "Bounties x" + std::to_string(bountyCount_);
    }
}

TransactionType TournamentPayout::type() const noexcept {
    return kind_ == Kind::Bounty ? TransactionType::BountyPayout : TransactionType::PrizePayout;
}

void TournamentPayout::apply(Cashbox& cashbox) const {
    cashbox.applyCount(cashPaid_, -1, MovementReason::Prize, playerId_, describe());
}

void TournamentPayout::reverse(Cashbox& cashbox) const {
    cashbox.applyCount(cashPaid_, +1, MovementReason::Undo, playerId_, "Undo " + describe());
}

std::string TournamentPayout::describe() const {
    if (kind_ == Kind::Bounty) {
        return "Bounty payout " + amount().toString() + " (" + std::to_string(bountyCount_) + ")";
    }
    return "Prize payout " + amount().toString() + " (place " + std::to_string(place_) + ")";
}

} // namespace pokersoc::domain
