#pragma once

namespace pokersoc::domain {
class DenominationCount;
class CashOut;
class DenominationTable;
class Cashbox;
class TransactionLog;
} // namespace pokersoc::domain

namespace pokersoc::persistence {

/// Abstract persistence boundary. The application layer records events here as
/// they happen, and asks it to rebuild the live objects on startup. Concrete
/// implementations (SQLite, in-memory, ...) are chosen in the composition root,
/// so the domain and services never depend on a specific storage engine.
class AppStore {
public:
    virtual ~AppStore() = default;

    /// Persist an opening / additional float (a tally of currency).
    virtual void recordFloat(const domain::DenominationCount& floatAmount) = 0;

    /// Persist a completed cash-out (chips in, cash out, tip).
    virtual void recordCashOut(const domain::CashOut& cashOut) = 0;

    /// Replay everything stored, rebuilding the cashbox and transaction log.
    virtual void load(const domain::DenominationTable& table,
                      domain::Cashbox& cashbox,
                      domain::TransactionLog& log) = 0;
};

} // namespace pokersoc::persistence
