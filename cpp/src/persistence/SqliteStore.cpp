#include "persistence/SqliteStore.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "domain/cashbox/Cashbox.h"
#include "domain/money/Denomination.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/DenominationTable.h"
#include "domain/money/Money.h"
#include "domain/transaction/CashOut.h"
#include "domain/transaction/TransactionLog.h"
#include "sqlite3.h"

namespace pokersoc::persistence {

using domain::CashOut;
using domain::Cashbox;
using domain::DenominationCount;
using domain::DenominationTable;
using domain::Money;
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
            }
        }

        if (event.kind == "FLOAT_ADD") {
            cashbox.applyFloat(floatCount);
        } else if (event.kind == "CASH_OUT") {
            auto cashOut = std::make_unique<CashOut>(
                event.playerId, chips, cash, Money::fromCents(event.tipCents));
            cashOut->apply(cashbox); // reproduces the payout movements
            log.add(std::move(cashOut));
        }
    }

    sqlite3_finalize(lineStatement);
}

} // namespace pokersoc::persistence
