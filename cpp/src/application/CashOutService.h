#pragma once

#include <memory>
#include <string>

#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"
#include "domain/transaction/CashOut.h"

namespace pokersoc::domain {
class Cashbox;
}

namespace pokersoc::application {

/// Use case: cash a player out. GUI-agnostic — the console (and later the GUI)
/// both call this. Values the returned chips, makes change from the cashbox,
/// applies the movements and hands back a recorded transaction.
class CashOutService {
public:
    struct Result {
        bool success = false;
        std::string message;
        std::unique_ptr<domain::CashOut> transaction; // set on success
        domain::DenominationCount change;              // cash handed to the player
    };

    explicit CashOutService(domain::Cashbox& cashbox) : cashbox_(cashbox) {}

    /// Cash out `chipsReturned` for `playerId`, keeping `tip` for the house.
    Result cashOut(const std::string& playerId,
                   const domain::DenominationCount& chipsReturned,
                   domain::Money tip = domain::Money{});

private:
    domain::Cashbox& cashbox_;
};

} // namespace pokersoc::application
