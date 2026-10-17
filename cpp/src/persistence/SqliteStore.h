#pragma once

#include <optional>
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
    void recordBuyIn(const domain::BuyIn& buyIn) override;
    void recordPlayer(const domain::Player& player) override;

    void saveSessionMode(domain::SessionModeType type) override;
    std::optional<domain::SessionModeType> loadSessionModeType() override;
    void saveCashGameConfig(const domain::CashGameConfig& config) override;
    domain::CashGameConfig loadCashGameConfig() override;
    void saveTournamentConfig(const domain::TournamentConfig& config) override;
    domain::TournamentConfig loadTournamentConfig() override;

    void load(const domain::DenominationTable& table,
              domain::Cashbox& cashbox,
              domain::TransactionLog& log) override;
    void loadPlayers(domain::PlayerDirectory& directory) override;

private:
    void exec(const char* sql);
    sqlite3_stmt* prepare(const char* sql);
    long long insertEvent(const char* kind, const std::string& playerId, long long tipCents);
    void insertLine(long long seq, const char* role, long long denomCents, int qty);

    void setSetting(const std::string& key, const std::string& value);
    std::optional<std::string> getSetting(const std::string& key);
    void setIntSetting(const std::string& key, long long value);
    long long getIntSetting(const std::string& key, long long fallback);

    sqlite3* db_ = nullptr;
};

} // namespace pokersoc::persistence
