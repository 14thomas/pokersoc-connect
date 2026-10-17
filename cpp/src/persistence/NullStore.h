#pragma once

#include "persistence/AppStore.h"

namespace pokersoc::persistence {

/// A store that persists nothing. Handy for tests or running the app with no
/// database file.
class NullStore final : public AppStore {
public:
    void recordFloat(const domain::DenominationCount&) override {}
    void recordCashOut(const domain::CashOut&) override {}
    void load(const domain::DenominationTable&, domain::Cashbox&, domain::TransactionLog&) override {}
};

} // namespace pokersoc::persistence
