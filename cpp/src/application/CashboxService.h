#pragma once

namespace pokersoc::domain {
class Cashbox;
class DenominationCount;
} // namespace pokersoc::domain

namespace pokersoc::persistence {
class AppStore;
}

namespace pokersoc::application {

/// Use case: manage the cashbox float. Persists the float before it touches the
/// in-memory cashbox, so the database stays the source of truth.
class CashboxService {
public:
    CashboxService(domain::Cashbox& cashbox, persistence::AppStore& store)
        : cashbox_(cashbox), store_(store) {}

    void addFloat(const domain::DenominationCount& amount);

private:
    domain::Cashbox& cashbox_;
    persistence::AppStore& store_;
};

} // namespace pokersoc::application
