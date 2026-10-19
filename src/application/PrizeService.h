#pragma once

#include <string>

#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"
#include "domain/transaction/TournamentPayout.h"

namespace pokersoc::application {

class Ledger;

/// Pays tournament placement prizes and bounties from the cashbox.
class PrizeService {
public:
    struct Result {
        bool success = false;
        std::string message;
        const domain::TournamentPayout* transaction = nullptr;
        domain::DenominationCount cash;
    };

    explicit PrizeService(Ledger& ledger) : ledger_(ledger) {}

    Result payPlacement(const std::string& playerId, int place, domain::Money amount);
    Result payBounty(const std::string& playerId, int count, domain::Money perBounty);

private:
    Result pay(const std::string& playerId, domain::TournamentPayout::Kind kind,
               domain::Money amount, int place, int bountyCount);

    Ledger& ledger_;
};

} // namespace pokersoc::application
