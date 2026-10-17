#pragma once

#include <optional>
#include <string>

#include "domain/session/CashGameMode.h"    // CashGameConfig
#include "domain/session/SessionMode.h"      // SessionModeType
#include "domain/session/TournamentConfig.h" // TournamentConfig

namespace pokersoc::domain {
class Attendance;
struct AttendanceRecord;
class Cashbox;
class DenominationTable;
class Player;
class PlayerDirectory;
struct Product;
class ProductCatalog;
class Transaction;
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

    // ---- transactions (an append-only event log) ----

    /// Persist any money transaction. Assigns its id and timestamp.
    virtual void recordTransaction(domain::Transaction& transaction) = 0;
    /// Mark a stored transaction as undone; it is skipped on future replays.
    virtual void voidTransaction(long long id) = 0;
    /// Replay every live transaction, rebuilding the cashbox and transaction log.
    virtual void load(const domain::DenominationTable& table,
                      domain::Cashbox& cashbox,
                      domain::TransactionLog& log) = 0;

    // ---- players ----

    /// Persist (insert or update) a player profile.
    virtual void recordPlayer(const domain::Player& player) = 0;
    /// Load every stored player into the directory.
    virtual void loadPlayers(domain::PlayerDirectory& directory) = 0;

    // ---- attendance (card scans) ----

    virtual domain::AttendanceRecord recordAttendance(const std::string& playerId) = 0;
    virtual void loadAttendance(domain::Attendance& attendance) = 0;

    // ---- food catalog ----

    /// Insert (id == 0) or update a product. Returns its id.
    virtual long long saveProduct(const domain::Product& product) = 0;
    /// Delete a product and clear any quick slot that pointed at it.
    virtual void deleteProduct(long long id) = 0;
    /// productId 0 empties the slot.
    virtual void saveFoodSlot(int slot, long long productId) = 0;
    virtual void loadCatalog(domain::ProductCatalog& catalog) = 0;

    // ---- key/value settings ----

    virtual void setSetting(const std::string& key, const std::string& value) = 0;
    virtual std::optional<std::string> getSetting(const std::string& key) = 0;

    // ---- maintenance ----

    /// Write a consistent copy of the whole store to `path`. Throws on failure.
    virtual void backupTo(const std::string& path) = 0;

    // ---- session configuration (built on the settings above) ----

    void saveSessionMode(domain::SessionModeType type);
    /// The stored mode, or nullopt if this is a brand-new session.
    std::optional<domain::SessionModeType> loadSessionModeType();

    void saveCashGameConfig(const domain::CashGameConfig& config);
    domain::CashGameConfig loadCashGameConfig();

    void saveTournamentConfig(const domain::TournamentConfig& config);
    domain::TournamentConfig loadTournamentConfig();

protected:
    void setIntSetting(const std::string& key, long long value);
    long long getIntSetting(const std::string& key, long long fallback);
};

} // namespace pokersoc::persistence
