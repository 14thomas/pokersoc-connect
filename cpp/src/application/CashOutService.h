#pragma once

#include <string>

#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"

namespace pokersoc::domain {
class Cashbox;
class CashOut;
class TransactionLog;
} // namespace pokersoc::domain

namespace pokersoc::persistence {
class AppStore;
}

namespace pokersoc::application {

/// Use case: cash a player out. GUI-agnostic — the console (and later the GUI)
/// both call this. Values the returned chips, makes change from the cashbox,
/// persists the transaction, applies the movements and records it in the log.
class CashOutService {
public:
    struct Result {
        bool success = false;
        std::string message;
        const domain::CashOut* transaction = nullptr; // owned by the log
        domain::DenominationCount change;              // cash handed to the player
    };

    CashOutService(domain::Cashbox& cashbox,
                   domain::TransactionLog& log,
                   persistence::AppStore& store)
        : cashbox_(cashbox), log_(log), store_(store) {}

    /// Cash out `chipsReturned` for `playerId`, keeping `tip` for the house.
    Result cashOut(const std::string& playerId,
                   const domain::DenominationCount& chipsReturned,
                   domain::Money tip = domain::Money{});

private:
    domain::Cashbox& cashbox_;
    domain::TransactionLog& log_;
    persistence::AppStore& store_;
};

} // namespace pokersoc::application
