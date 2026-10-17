#include "persistence/SqliteStore.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "domain/cashbox/Cashbox.h"
#include "domain/money/Denomination.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/DenominationTable.h"
#include "domain/money/Money.h"
#include "domain/player/Player.h"
#include "domain/player/PlayerDirectory.h"
#include "domain/transaction/BuyIn.h"
#include "domain/transaction/CashOut.h"
#include "domain/transaction/TransactionLog.h"
#include "sqlite3.h"

namespace pokersoc::persistence {

using domain::BuyIn;
using domain::CashGameConfig;
using domain::CashOut;
using domain::Cashbox;
using domain::DenominationCount;
using domain::DenominationTable;
using domain::Money;
using domain::Player;
using domain::PlayerDirectory;
using domain::Rebuy;
using domain::SessionModeType;
using domain::TournamentConfig;
using domain::TransactionLog;

namespace {

constexpr const char* kSchema = R"sql(
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS event (
  seq        INTEGER PRIMARY KEY AUTOINCREMENT,
  kind       TEXT NOT NULL,                      -- 'FLOAT_ADD' | 'CASH_OUT'
  player_id  TEXT,
  tip_cents  INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS event_line (
  line_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  seq         INTEGER NOT NULL REFERENCES event(seq) ON DELETE CASCADE,
  role        TEXT NOT NULL,                      -- 'FLOAT' | 'CHIP' | 'CASH'
  denom_cents INTEGER NOT NULL,
  qty         INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_event_line_seq ON event_line(seq);

CREATE TABLE IF NOT EXISTS player (
  id          TEXT PRIMARY KEY,
  name        TEXT NOT NULL DEFAULT '',
  email       TEXT NOT NULL DEFAULT '',
  student_no  TEXT NOT NULL DEFAULT '',
  degree      TEXT NOT NULL DEFAULT '',
  study_year  INTEGER NOT NULL DEFAULT 0,
  arc_status  TEXT NOT NULL DEFAULT 'UNKNOWN',
  membership  TEXT NOT NULL DEFAULT 'NONE',
  underage    INTEGER NOT NULL DEFAULT 0,
  created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Key/value session settings: which mode this session runs and its rules.
CREATE TABLE IF NOT EXISTS app_setting (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL
);
)sql";

std::string columnText(sqlite3_stmt* statement, int index) {
    const unsigned char* text = sqlite3_column_text(statement, index);
    return text ? reinterpret_cast<const char*>(text) : std::string();
}

} // namespace

SqliteStore::SqliteStore(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        const std::string message = db_ ? sqlite3_errmsg(db_) : "open failed";
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("SqliteStore: cannot open '" + path + "': " + message);
    }
    exec(kSchema);
}

SqliteStore::~SqliteStore() {
    sqlite3_close(db_);
}

void SqliteStore::exec(const char* sql) {
    char* error = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "unknown error";
        sqlite3_free(error);
        throw std::runtime_error("SqliteStore: exec failed: " + message);
    }
}

sqlite3_stmt* SqliteStore::prepare(const char* sql) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("SqliteStore: prepare failed: ") +
                                 sqlite3_errmsg(db_));
    }
    return statement;
}

long long SqliteStore::insertEvent(const char* kind, const std::string& playerId,
                                   long long tipCents) {
    sqlite3_stmt* statement =
        prepare("INSERT INTO event(kind, player_id, tip_cents) VALUES(?, ?, ?)");
    sqlite3_bind_text(statement, 1, kind, -1, SQLITE_TRANSIENT);
    if (playerId.empty()) {
        sqlite3_bind_null(statement, 2);
    } else {
        sqlite3_bind_text(statement, 2, playerId.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int64(statement, 3, tipCents);

    const int rc = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: insert event failed");
    }
    return sqlite3_last_insert_rowid(db_);
}

void SqliteStore::insertLine(long long seq, const char* role, long long denomCents, int qty) {
    sqlite3_stmt* statement =
        prepare("INSERT INTO event_line(seq, role, denom_cents, qty) VALUES(?, ?, ?, ?)");
    sqlite3_bind_int64(statement, 1, seq);
    sqlite3_bind_text(statement, 2, role, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement, 3, denomCents);
    sqlite3_bind_int(statement, 4, qty);

    const int rc = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: insert line failed");
    }
}

void SqliteStore::recordFloat(const DenominationCount& floatAmount) {
    if (floatAmount.empty()) {
        return;
    }
    exec("BEGIN");
    try {
        const long long seq = insertEvent("FLOAT_ADD", "", 0);
        for (const auto& entry : floatAmount.entries()) {
            insertLine(seq, "FLOAT", entry.denomination->cents(), entry.qty);
        }
        exec("COMMIT");
    } catch (...) {
        exec("ROLLBACK");
        throw;
    }
}

void SqliteStore::recordCashOut(const CashOut& cashOut) {
    exec("BEGIN");
    try {
        const long long seq =
            insertEvent("CASH_OUT", cashOut.playerId(), cashOut.tip().cents());
        for (const auto& entry : cashOut.chipsReturned().entries()) {
            insertLine(seq, "CHIP", entry.denomination->cents(), entry.qty);
        }
        for (const auto& entry : cashOut.cashPaidOut().entries()) {
            insertLine(seq, "CASH", entry.denomination->cents(), entry.qty);
        }
        exec("COMMIT");
    } catch (...) {
        exec("ROLLBACK");
        throw;
    }
}

void SqliteStore::recordBuyIn(const BuyIn& buyIn) {
    exec("BEGIN");
    try {
        const char* kind = buyIn.isRebuy() ? "REBUY" : "BUY_IN";
        const long long seq = insertEvent(kind, buyIn.playerId(), 0);
        for (const auto& entry : buyIn.cashReceived().entries()) {
            insertLine(seq, "CASH_IN", entry.denomination->cents(), entry.qty);
        }
        for (const auto& entry : buyIn.changeGiven().entries()) {
            insertLine(seq, "CHANGE", entry.denomination->cents(), entry.qty);
        }
        exec("COMMIT");
    } catch (...) {
        exec("ROLLBACK");
        throw;
    }
}

void SqliteStore::recordPlayer(const Player& player) {
    // Upsert by id. The database owns `created_at`, so an update leaves the
    // original creation time untouched while refreshing the editable profile.
    sqlite3_stmt* statement = prepare(
        "INSERT INTO player(id, name, email, student_no, degree, study_year, "
        "arc_status, membership, underage) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "name = excluded.name, email = excluded.email, "
        "student_no = excluded.student_no, degree = excluded.degree, "
        "study_year = excluded.study_year, arc_status = excluded.arc_status, "
        "membership = excluded.membership, underage = excluded.underage");

    sqlite3_bind_text(statement, 1, player.id().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, player.name().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, player.email().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, player.studentNumber().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, player.degree().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 6, player.studyYear());
    sqlite3_bind_text(statement, 7, domain::arcStatusName(player.arcStatus()), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 8, domain::membershipTypeName(player.membership()), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 9, player.isUnderage() ? 1 : 0);

    const int rc = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: insert player failed");
    }
}

void SqliteStore::setSetting(const std::string& key, const std::string& value) {
    sqlite3_stmt* statement = prepare(
        "INSERT INTO app_setting(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    sqlite3_bind_text(statement, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: set setting failed");
    }
}

std::optional<std::string> SqliteStore::getSetting(const std::string& key) {
    sqlite3_stmt* statement = prepare("SELECT value FROM app_setting WHERE key = ?");
    sqlite3_bind_text(statement, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<std::string> value;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        value = columnText(statement, 0);
    }
    sqlite3_finalize(statement);
    return value;
}

void SqliteStore::setIntSetting(const std::string& key, long long value) {
    setSetting(key, std::to_string(value));
}

long long SqliteStore::getIntSetting(const std::string& key, long long fallback) {
    const auto value = getSetting(key);
    if (!value) {
        return fallback;
    }
    try {
        return std::stoll(*value);
    } catch (...) {
        return fallback;
    }
}

void SqliteStore::saveSessionMode(SessionModeType type) {
    setSetting("session_mode", domain::sessionModeTypeName(type));
}

std::optional<SessionModeType> SqliteStore::loadSessionModeType() {
    const auto value = getSetting("session_mode");
    if (!value) {
        return std::nullopt;
    }
    return domain::sessionModeTypeFromName(*value);
}

void SqliteStore::saveCashGameConfig(const CashGameConfig& config) {
    setIntSetting("cash.min_cents", config.minBuyIn.cents());
    setIntSetting("cash.max_cents", config.maxBuyIn.cents());
}

CashGameConfig SqliteStore::loadCashGameConfig() {
    CashGameConfig config; // defaults
    config.minBuyIn = Money::fromCents(getIntSetting("cash.min_cents", config.minBuyIn.cents()));
    config.maxBuyIn = Money::fromCents(getIntSetting("cash.max_cents", config.maxBuyIn.cents()));
    return config;
}

void SqliteStore::saveTournamentConfig(const TournamentConfig& config) {
    setSetting("tnmt.pricing", domain::tournamentPricingName(config.pricing));
    setIntSetting("tnmt.arc_only", config.arcOnly ? 1 : 0);
    setIntSetting("tnmt.rebuy_cap", config.rebuyCap);

    setIntSetting("tnmt.arc_buyin", config.arcBuyInCents);
    setIntSetting("tnmt.nonarc_buyin", config.nonArcBuyInCents);
    setIntSetting("tnmt.arc_rebuy", config.arcRebuyCents);
    setIntSetting("tnmt.nonarc_rebuy", config.nonArcRebuyCents);

    setIntSetting("tnmt.bronze_buyin", config.bronzeBuyInCents);
    setIntSetting("tnmt.silver_buyin", config.silverBuyInCents);
    setIntSetting("tnmt.gold_buyin", config.goldBuyInCents);
    setIntSetting("tnmt.platinum_buyin", config.platinumBuyInCents);
    setIntSetting("tnmt.diamond_buyin", config.diamondBuyInCents);

    setIntSetting("tnmt.bronze_rebuy", config.bronzeRebuyCents);
    setIntSetting("tnmt.silver_rebuy", config.silverRebuyCents);
    setIntSetting("tnmt.gold_rebuy", config.goldRebuyCents);
    setIntSetting("tnmt.platinum_rebuy", config.platinumRebuyCents);
    setIntSetting("tnmt.diamond_rebuy", config.diamondRebuyCents);
}

TournamentConfig SqliteStore::loadTournamentConfig() {
    TournamentConfig config; // defaults
    if (const auto pricing = getSetting("tnmt.pricing")) {
        config.pricing = domain::tournamentPricingFromName(*pricing);
    }
    config.arcOnly = getIntSetting("tnmt.arc_only", config.arcOnly ? 1 : 0) != 0;
    config.rebuyCap = static_cast<int>(getIntSetting("tnmt.rebuy_cap", config.rebuyCap));

    config.arcBuyInCents = getIntSetting("tnmt.arc_buyin", config.arcBuyInCents);
    config.nonArcBuyInCents = getIntSetting("tnmt.nonarc_buyin", config.nonArcBuyInCents);
    config.arcRebuyCents = getIntSetting("tnmt.arc_rebuy", config.arcRebuyCents);
    config.nonArcRebuyCents = getIntSetting("tnmt.nonarc_rebuy", config.nonArcRebuyCents);

    config.bronzeBuyInCents = getIntSetting("tnmt.bronze_buyin", config.bronzeBuyInCents);
    config.silverBuyInCents = getIntSetting("tnmt.silver_buyin", config.silverBuyInCents);
    config.goldBuyInCents = getIntSetting("tnmt.gold_buyin", config.goldBuyInCents);
    config.platinumBuyInCents = getIntSetting("tnmt.platinum_buyin", config.platinumBuyInCents);
    config.diamondBuyInCents = getIntSetting("tnmt.diamond_buyin", config.diamondBuyInCents);

    config.bronzeRebuyCents = getIntSetting("tnmt.bronze_rebuy", config.bronzeRebuyCents);
    config.silverRebuyCents = getIntSetting("tnmt.silver_rebuy", config.silverRebuyCents);
    config.goldRebuyCents = getIntSetting("tnmt.gold_rebuy", config.goldRebuyCents);
    config.platinumRebuyCents = getIntSetting("tnmt.platinum_rebuy", config.platinumRebuyCents);
    config.diamondRebuyCents = getIntSetting("tnmt.diamond_rebuy", config.diamondRebuyCents);
    return config;
}

void SqliteStore::load(const DenominationTable& table, Cashbox& cashbox, TransactionLog& log) {
    struct Event {
        long long seq;
        std::string kind;
        std::string playerId;
        long long tipCents;
    };

    std::vector<Event> events;
    {
        sqlite3_stmt* statement =
            prepare("SELECT seq, kind, player_id, tip_cents FROM event ORDER BY seq");
        while (sqlite3_step(statement) == SQLITE_ROW) {
            events.push_back(Event{sqlite3_column_int64(statement, 0),
                                   columnText(statement, 1),
                                   columnText(statement, 2),
                                   sqlite3_column_int64(statement, 3)});
        }
        sqlite3_finalize(statement);
    }

    sqlite3_stmt* lineStatement =
        prepare("SELECT role, denom_cents, qty FROM event_line WHERE seq = ? ORDER BY line_id");

    for (const auto& event : events) {
        DenominationCount floatCount;
        DenominationCount chips;
        DenominationCount cash;
        DenominationCount cashIn;  // buy-in: cash handed over
        DenominationCount change;  // buy-in: change returned

        sqlite3_reset(lineStatement);
        sqlite3_bind_int64(lineStatement, 1, event.seq);
        while (sqlite3_step(lineStatement) == SQLITE_ROW) {
            const std::string role = columnText(lineStatement, 0);
            const long long denomCents = sqlite3_column_int64(lineStatement, 1);
            const int qty = sqlite3_column_int(lineStatement, 2);

            if (role == "FLOAT") {
                if (const auto denomination = table.findCurrency(denomCents)) {
                    floatCount.add(denomination, qty);
                }
            } else if (role == "CHIP") {
                if (const auto denomination = table.findChip(denomCents)) {
                    chips.add(denomination, qty);
                }
            } else if (role == "CASH") {
                if (const auto denomination = table.findCurrency(denomCents)) {
                    cash.add(denomination, qty);
                }
            } else if (role == "CASH_IN") {
                if (const auto denomination = table.findCurrency(denomCents)) {
                    cashIn.add(denomination, qty);
                }
            } else if (role == "CHANGE") {
                if (const auto denomination = table.findCurrency(denomCents)) {
                    change.add(denomination, qty);
                }
            }
        }

        if (event.kind == "FLOAT_ADD") {
            cashbox.applyFloat(floatCount);
        } else if (event.kind == "CASH_OUT") {
            auto cashOut = std::make_unique<CashOut>(
                event.playerId, chips, cash, Money::fromCents(event.tipCents));
            cashOut->apply(cashbox); // reproduces the payout movements
            log.add(std::move(cashOut));
        } else if (event.kind == "BUY_IN" || event.kind == "REBUY") {
            const Money buyInAmount = cashIn.total() - change.total();
            std::unique_ptr<BuyIn> buyIn =
                event.kind == "REBUY"
                    ? std::make_unique<Rebuy>(event.playerId, cashIn, change, buyInAmount)
                    : std::make_unique<BuyIn>(event.playerId, cashIn, change, buyInAmount);
            buyIn->apply(cashbox); // reproduces the cash-in / change movements
            log.add(std::move(buyIn));
        }
    }

    sqlite3_finalize(lineStatement);
}

void SqliteStore::loadPlayers(PlayerDirectory& directory) {
    sqlite3_stmt* statement = prepare(
        "SELECT id, name, email, student_no, degree, study_year, arc_status, "
        "membership, underage, created_at FROM player ORDER BY id");

    while (sqlite3_step(statement) == SQLITE_ROW) {
        Player player(columnText(statement, 0), columnText(statement, 1));
        player.setEmail(columnText(statement, 2));
        player.setStudentNumber(columnText(statement, 3));
        player.setDegree(columnText(statement, 4));
        player.setStudyYear(sqlite3_column_int(statement, 5));
        player.setArcStatus(domain::arcStatusFromName(columnText(statement, 6)));
        player.setMembership(domain::membershipTypeFromName(columnText(statement, 7)));
        player.setUnderage(sqlite3_column_int(statement, 8) != 0);
        player.setCreatedAt(columnText(statement, 9));
        directory.upsert(std::move(player));
    }

    sqlite3_finalize(statement);
}

} // namespace pokersoc::persistence
