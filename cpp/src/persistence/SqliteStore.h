#pragma once

#include <string>

#include "persistence/AppStore.h"

struct sqlite3;
struct sqlite3_stmt;

namespace pokersoc::persistence {

/// SQLite-backed store using an event-sourced schema: each float / cash-out is
/// appended as an `event` with its denomination `event_line`s. Rebuilding the
/// live objects is a replay of those events in order, which reproduces the
/// cashbox stock, its movement ledger and the transaction history exactly.
class SqliteStore final : public AppStore {
public:
    /// Opens (creating if needed) the database at `path` and ensures the schema.
    explicit SqliteStore(const std::string& path);
    ~SqliteStore() override;

    SqliteStore(const SqliteStore&) = delete;
    SqliteStore& operator=(const SqliteStore&) = delete;

    void recordFloat(const domain::DenominationCount& floatAmount) override;
    void recordCashOut(const domain::CashOut& cashOut) override;
    void load(const domain::DenominationTable& table,
              domain::Cashbox& cashbox,
              domain::TransactionLog& log) override;

private:
    void exec(const char* sql);
    sqlite3_stmt* prepare(const char* sql);
    long long insertEvent(const char* kind, const std::string& playerId, long long tipCents);
    void insertLine(long long seq, const char* role, long long denomCents, int qty);

    sqlite3* db_ = nullptr;
};

} // namespace pokersoc::persistence
