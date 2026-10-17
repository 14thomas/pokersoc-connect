#pragma once

#include <optional>
#include <string>

#include "persistence/AppStore.h"

struct sqlite3;
struct sqlite3_stmt;

namespace pokersoc::persistence {

/// SQLite-backed store using an event-sourced schema: each money transaction is
/// appended as an `event` with its denomination `event_line`s. Rebuilding the
/// live objects is a replay of those events in order, which reproduces the
/// cashbox stock, its movement ledger and the transaction history exactly.
/// Undo marks an event `voided` rather than deleting it, so history is kept.
class SqliteStore final : public AppStore {
public:
    /// Opens (creating if needed) the database at `path` and ensures the schema.
    explicit SqliteStore(const std::string& path);
    ~SqliteStore() override;

    SqliteStore(const SqliteStore&) = delete;
    SqliteStore& operator=(const SqliteStore&) = delete;

    void recordTransaction(domain::Transaction& transaction) override;
    void voidTransaction(long long id) override;
    void load(const domain::DenominationTable& table,
              domain::Cashbox& cashbox,
              domain::TransactionLog& log) override;

    void recordPlayer(const domain::Player& player) override;
    void loadPlayers(domain::PlayerDirectory& directory) override;

    domain::AttendanceRecord recordAttendance(const std::string& playerId) override;
    void loadAttendance(domain::Attendance& attendance) override;

    long long saveProduct(const domain::Product& product) override;
    void deleteProduct(long long id) override;
    void saveFoodSlot(int slot, long long productId) override;
    void loadCatalog(domain::ProductCatalog& catalog) override;

    void setSetting(const std::string& key, const std::string& value) override;
    std::optional<std::string> getSetting(const std::string& key) override;

    void backupTo(const std::string& path) override;

private:
    void exec(const char* sql);
    sqlite3_stmt* prepare(const char* sql);
    bool hasColumn(const char* table, const char* column);
    void migrate();

    long long insertEvent(const char* kind, const std::string& playerId, long long tipCents,
                          const std::string& notes, const char* method, const std::string& time);
    void insertLine(long long seq, const char* role, long long denomCents, int qty,
                    const std::string& label = "");

    sqlite3* db_ = nullptr;
};

} // namespace pokersoc::persistence
