#include "persistence/SqliteStore.h"

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "domain/cashbox/Cashbox.h"
#include "domain/food/Product.h"
#include "domain/food/ProductCatalog.h"
#include "domain/food/SaleLine.h"
#include "domain/money/Denomination.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/DenominationTable.h"
#include "domain/money/Money.h"
#include "domain/player/Attendance.h"
#include "domain/player/Player.h"
#include "domain/player/PlayerDirectory.h"
#include "domain/transaction/BuyIn.h"
#include "domain/transaction/CashOut.h"
#include "domain/transaction/CashboxTransactions.h"
#include "domain/transaction/FoodSale.h"
#include "domain/transaction/TournamentPayout.h"
#include "domain/transaction/TransactionLog.h"
#include "sqlite3.h"
#include "util/Time.h"

namespace pokersoc::persistence {

using domain::BuyIn;
using domain::CashOut;
using domain::CashTip;
using domain::Cashbox;
using domain::DenominationCount;
using domain::DenominationTable;
using domain::Exchange;
using domain::FloatAdd;
using domain::FoodSale;
using domain::LostChips;
using domain::Money;
using domain::PaymentMethod;
using domain::Player;
using domain::PlayerDirectory;
using domain::Rebuy;
using domain::SaleLine;
using domain::TournamentPayout;
using domain::Transaction;
using domain::TransactionLog;
using domain::TransactionType;

namespace {

constexpr const char* kSchema = R"sql(
PRAGMA foreign_keys = ON;

-- One row per money transaction. `kind` is one of:
--   FLOAT_ADD, BUY_IN, REBUY, CASH_OUT, TIP, LOST_CHIPS, EXCHANGE, FOOD_SALE
CREATE TABLE IF NOT EXISTS event (
  seq        INTEGER PRIMARY KEY AUTOINCREMENT,
  kind       TEXT NOT NULL,
  player_id  TEXT,
  tip_cents  INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- The denomination tallies of an event. `role` says which tally a line is in
-- (FLOAT, CASH_IN, CHANGE, CHIP, CASH, EXTRA_CASH, GIVEN, RECEIVED, PAY, FOOD).
-- FOOD lines store the unit price in denom_cents and the product name in label.
CREATE TABLE IF NOT EXISTS event_line (
  line_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  seq         INTEGER NOT NULL REFERENCES event(seq) ON DELETE CASCADE,
  role        TEXT NOT NULL,
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

CREATE TABLE IF NOT EXISTS attendance (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  player_id  TEXT NOT NULL,
  scanned_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS product (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  name        TEXT NOT NULL,
  price_cents INTEGER NOT NULL DEFAULT 0,
  image       TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS food_slot (
  slot       INTEGER PRIMARY KEY,
  product_id INTEGER NOT NULL
);
)sql";

std::string columnText(sqlite3_stmt* statement, int index) {
    const unsigned char* text = sqlite3_column_text(statement, index);
    return text ? reinterpret_cast<const char*>(text) : std::string();
}

/// Finalizes a prepared statement when it goes out of scope.
struct StatementGuard {
    sqlite3_stmt* statement;
    ~StatementGuard() { sqlite3_finalize(statement); }
};

void bindText(sqlite3_stmt* statement, int index, const std::string& text) {
    sqlite3_bind_text(statement, index, text.c_str(), -1, SQLITE_TRANSIENT);
}

const char* eventKind(TransactionType type) {
    switch (type) {
        case TransactionType::BuyIn:     return "BUY_IN";
        case TransactionType::Rebuy:     return "REBUY";
        case TransactionType::CashOut:   return "CASH_OUT";
        case TransactionType::FloatAdd:  return "FLOAT_ADD";
        case TransactionType::CashTip:   return "TIP";
        case TransactionType::LostChips: return "LOST_CHIPS";
        case TransactionType::Exchange:  return "EXCHANGE";
        case TransactionType::FoodSale:     return "FOOD_SALE";
        case TransactionType::PrizePayout:  return "PRIZE_PAYOUT";
        case TransactionType::BountyPayout: return "BOUNTY_PAYOUT";
    }
    return "UNKNOWN";
}

} // namespace

SqliteStore::SqliteStore(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        const std::string message = db_ ? sqlite3_errmsg(db_) : "open failed";
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("SqliteStore: cannot open '" + path + "': " + message);
    }
    try {
        exec(kSchema);
        migrate();
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
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

bool SqliteStore::hasColumn(const char* table, const char* column) {
    const std::string sql = std::string("PRAGMA table_info(") + table + ")";
    StatementGuard guard{prepare(sql.c_str())};
    while (sqlite3_step(guard.statement) == SQLITE_ROW) {
        if (columnText(guard.statement, 1) == column) {
            return true;
        }
    }
    return false;
}

// Session files created by earlier builds lack the newer columns; add them in
// place so old sessions keep opening.
void SqliteStore::migrate() {
    if (!hasColumn("event", "notes")) {
        exec("ALTER TABLE event ADD COLUMN notes TEXT NOT NULL DEFAULT ''");
    }
    if (!hasColumn("event", "voided")) {
        exec("ALTER TABLE event ADD COLUMN voided INTEGER NOT NULL DEFAULT 0");
    }
    if (!hasColumn("event", "method")) {
        exec("ALTER TABLE event ADD COLUMN method TEXT NOT NULL DEFAULT ''");
    }
    if (!hasColumn("event_line", "label")) {
        exec("ALTER TABLE event_line ADD COLUMN label TEXT NOT NULL DEFAULT ''");
    }
}

long long SqliteStore::insertEvent(const char* kind, const std::string& playerId,
                                   long long tipCents, const std::string& notes,
                                   const char* method, const std::string& time) {
    StatementGuard guard{prepare(
        "INSERT INTO event(kind, player_id, tip_cents, notes, method, created_at) "
        "VALUES(?, ?, ?, ?, ?, ?)")};
    sqlite3_bind_text(guard.statement, 1, kind, -1, SQLITE_TRANSIENT);
    if (playerId.empty()) {
        sqlite3_bind_null(guard.statement, 2);
    } else {
        bindText(guard.statement, 2, playerId);
    }
    sqlite3_bind_int64(guard.statement, 3, tipCents);
    bindText(guard.statement, 4, notes);
    sqlite3_bind_text(guard.statement, 5, method, -1, SQLITE_TRANSIENT);
    bindText(guard.statement, 6, time);

    if (sqlite3_step(guard.statement) != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: insert event failed");
    }
    return sqlite3_last_insert_rowid(db_);
}

void SqliteStore::insertLine(long long seq, const char* role, long long denomCents, int qty,
                             const std::string& label) {
    StatementGuard guard{prepare(
        "INSERT INTO event_line(seq, role, denom_cents, qty, label) VALUES(?, ?, ?, ?, ?)")};
    sqlite3_bind_int64(guard.statement, 1, seq);
    sqlite3_bind_text(guard.statement, 2, role, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(guard.statement, 3, denomCents);
    sqlite3_bind_int(guard.statement, 4, qty);
    bindText(guard.statement, 5, label);

    if (sqlite3_step(guard.statement) != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: insert line failed");
    }
}

void SqliteStore::recordTransaction(Transaction& transaction) {
    const std::string time = util::nowUtc();

    auto writeCount = [this](long long seq, const char* role, const DenominationCount& count) {
        for (const auto& entry : count.entries()) {
            insertLine(seq, role, entry.denomination->cents(), entry.qty);
        }
    };
    auto writeFood = [this](long long seq, const std::vector<SaleLine>& lines) {
        for (const auto& line : lines) {
            insertLine(seq, "FOOD", line.unitPrice.cents(), line.qty, line.productName);
        }
    };

    exec("BEGIN");
    try {
        const char* kind = eventKind(transaction.type());
        long long seq = 0;

        switch (transaction.type()) {
            case TransactionType::BuyIn:
            case TransactionType::Rebuy: {
                const auto& buyIn = static_cast<const BuyIn&>(transaction);
                seq = insertEvent(kind, buyIn.playerId(), 0, buyIn.notes(), "", time);
                writeCount(seq, "CASH_IN", buyIn.cashReceived());
                writeCount(seq, "CHANGE", buyIn.changeGiven());
                insertLine(seq, "SPLIT", buyIn.prizeAmount().cents(), 1, "PRIZE");
                insertLine(seq, "SPLIT", buyIn.bountyAmount().cents(), 1, "BOUNTY");
                insertLine(seq, "SPLIT", buyIn.rakeAmount().cents(), 1, "RAKE");
                break;
            }
            case TransactionType::CashOut: {
                const auto& cashOut = static_cast<const CashOut&>(transaction);
                seq = insertEvent(kind, cashOut.playerId(), cashOut.tip().cents(),
                                  cashOut.notes(), "", time);
                writeCount(seq, "CHIP", cashOut.chipsReturned());
                writeCount(seq, "CASH", cashOut.cashPaidOut());
                writeCount(seq, "EXTRA_CASH", cashOut.extraCash());
                writeFood(seq, cashOut.food());
                break;
            }
            case TransactionType::FloatAdd: {
                const auto& floatAdd = static_cast<const FloatAdd&>(transaction);
                seq = insertEvent(kind, "", 0, floatAdd.notes(), "", time);
                writeCount(seq, "FLOAT", floatAdd.cash());
                break;
            }
            case TransactionType::CashTip: {
                const auto& tip = static_cast<const CashTip&>(transaction);
                seq = insertEvent(kind, tip.playerId(), 0, tip.notes(), "", time);
                writeCount(seq, "CASH", tip.cash());
                break;
            }
            case TransactionType::LostChips: {
                const auto& lost = static_cast<const LostChips&>(transaction);
                seq = insertEvent(kind, "", 0, lost.notes(), "", time);
                writeCount(seq, "CHIP", lost.chips());
                break;
            }
            case TransactionType::Exchange: {
                const auto& exchange = static_cast<const Exchange&>(transaction);
                seq = insertEvent(kind, "", 0, exchange.notes(), "", time);
                writeCount(seq, "GIVEN", exchange.given());
                writeCount(seq, "RECEIVED", exchange.received());
                break;
            }
            case TransactionType::FoodSale: {
                const auto& sale = static_cast<const FoodSale&>(transaction);
                seq = insertEvent(kind, sale.playerId(), 0, sale.notes(),
                                  domain::paymentMethodName(sale.method()), time);
                writeFood(seq, sale.lines());
                writeCount(seq, "PAY", sale.payment());
                writeCount(seq, "CHANGE", sale.change());
                break;
            }
            case TransactionType::PrizePayout:
            case TransactionType::BountyPayout: {
                const auto& payout = static_cast<const TournamentPayout&>(transaction);
                const long long extra = payout.kind() == TournamentPayout::Kind::Bounty
                                            ? payout.bountyCount()
                                            : payout.place();
                seq = insertEvent(kind, payout.playerId(), extra, payout.notes(), "", time);
                writeCount(seq, "CASH", payout.cashPaid());
                break;
            }
        }

        exec("COMMIT");
        transaction.setId(seq);
        transaction.setTime(time);
    } catch (...) {
        exec("ROLLBACK");
        throw;
    }
}

void SqliteStore::voidTransaction(long long id) {
    StatementGuard guard{prepare("UPDATE event SET voided = 1 WHERE seq = ?")};
    sqlite3_bind_int64(guard.statement, 1, id);
    if (sqlite3_step(guard.statement) != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: void event failed");
    }
}

void SqliteStore::load(const DenominationTable& table, Cashbox& cashbox, TransactionLog& log) {
    struct Event {
        long long seq;
        std::string kind;
        std::string playerId;
        long long tipCents;
        std::string createdAt;
        std::string notes;
        std::string method;
    };

    std::vector<Event> events;
    {
        StatementGuard guard{prepare(
            "SELECT seq, kind, player_id, tip_cents, created_at, notes, method "
            "FROM event WHERE voided = 0 ORDER BY seq")};
        while (sqlite3_step(guard.statement) == SQLITE_ROW) {
            events.push_back(Event{sqlite3_column_int64(guard.statement, 0),
                                   columnText(guard.statement, 1),
                                   columnText(guard.statement, 2),
                                   sqlite3_column_int64(guard.statement, 3),
                                   columnText(guard.statement, 4),
                                   columnText(guard.statement, 5),
                                   columnText(guard.statement, 6)});
        }
    }

    StatementGuard lines{prepare(
        "SELECT role, denom_cents, qty, label FROM event_line WHERE seq = ? ORDER BY line_id")};

    cashbox.setAllowOverdraw(true);
    try {
        for (const auto& event : events) {
            const bool chipPayment =
                event.kind == "FOOD_SALE" &&
                domain::paymentMethodFromName(event.method) == PaymentMethod::Chips;

            // role -> tally, resolving each line against cash or chip denominations
            std::map<std::string, DenominationCount> tallies;
            std::map<std::string, long long> splits;
            std::vector<SaleLine> food;
            auto tally = [&tallies](const std::string& role) -> const DenominationCount& {
                return tallies[role];
            };

            sqlite3_reset(lines.statement);
            sqlite3_bind_int64(lines.statement, 1, event.seq);
            while (sqlite3_step(lines.statement) == SQLITE_ROW) {
                const std::string role = columnText(lines.statement, 0);
                const long long denomCents = sqlite3_column_int64(lines.statement, 1);
                const int qty = sqlite3_column_int(lines.statement, 2);

                if (role == "FOOD") {
                    food.push_back(SaleLine{columnText(lines.statement, 3),
                                            Money::fromCents(denomCents), qty});
                    continue;
                }
                if (role == "SPLIT") {
                    splits[columnText(lines.statement, 3)] = denomCents;
                    continue;
                }
                const bool isChip =
                    role == "CHIP" || (chipPayment && (role == "PAY" || role == "CHANGE"));
                const auto denomination =
                    isChip ? table.findChip(denomCents) : table.findCurrency(denomCents);
                if (denomination) {
                    tallies[role].add(denomination, qty);
                }
            }

            std::unique_ptr<Transaction> transaction;
            if (event.kind == "FLOAT_ADD") {
                transaction = std::make_unique<FloatAdd>(tally("FLOAT"));
            } else if (event.kind == "CASH_OUT") {
                transaction = std::make_unique<CashOut>(
                    event.playerId, tally("CHIP"), tally("CASH"),
                    Money::fromCents(event.tipCents), tally("EXTRA_CASH"), std::move(food));
            } else if (event.kind == "BUY_IN" || event.kind == "REBUY") {
                const DenominationCount& cashIn = tally("CASH_IN");
                const DenominationCount& change = tally("CHANGE");
                const Money amount = cashIn.total() - change.total();
                if (event.kind == "REBUY") {
                    transaction = std::make_unique<Rebuy>(event.playerId, cashIn, change, amount);
                } else {
                    transaction = std::make_unique<BuyIn>(event.playerId, cashIn, change, amount);
                }
                if (!splits.empty()) {
                    static_cast<BuyIn&>(*transaction)
                        .setSplit(Money::fromCents(splits["PRIZE"]),
                                  Money::fromCents(splits["BOUNTY"]),
                                  Money::fromCents(splits["RAKE"]));
                }
            } else if (event.kind == "PRIZE_PAYOUT" || event.kind == "BOUNTY_PAYOUT") {
                const auto kind = event.kind == "BOUNTY_PAYOUT" ? TournamentPayout::Kind::Bounty
                                                               : TournamentPayout::Kind::Placement;
                const int extra = static_cast<int>(event.tipCents);
                transaction = std::make_unique<TournamentPayout>(
                    event.playerId, kind, tally("CASH"),
                    kind == TournamentPayout::Kind::Placement ? extra : 0,
                    kind == TournamentPayout::Kind::Bounty ? extra : 0);
            } else if (event.kind == "TIP") {
                transaction = std::make_unique<CashTip>(tally("CASH"), event.playerId);
            } else if (event.kind == "LOST_CHIPS") {
                transaction = std::make_unique<LostChips>(tally("CHIP"));
            } else if (event.kind == "EXCHANGE") {
                transaction = std::make_unique<Exchange>(tally("GIVEN"), tally("RECEIVED"));
            } else if (event.kind == "FOOD_SALE") {
                transaction = std::make_unique<FoodSale>(
                    event.playerId, std::move(food), domain::paymentMethodFromName(event.method),
                    tally("PAY"), tally("CHANGE"));
            } else {
                continue; // unknown kind from a newer build: skip rather than fail
            }

            transaction->setId(event.seq);
            transaction->setTime(event.createdAt);
            if (!event.notes.empty()) {
                transaction->setNotes(event.notes);
            }
            transaction->apply(cashbox); // reproduces the cashbox movements
            log.add(std::move(transaction));
        }
    } catch (...) {
        cashbox.setAllowOverdraw(false);
        throw;
    }
    cashbox.setAllowOverdraw(false);
}

// ---- players ----

void SqliteStore::recordPlayer(const Player& player) {
    // Upsert by id. The database owns `created_at`, so an update leaves the
    // original creation time untouched while refreshing the editable profile.
    StatementGuard guard{prepare(
        "INSERT INTO player(id, name, email, student_no, degree, study_year, "
        "arc_status, membership, underage) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "name = excluded.name, email = excluded.email, "
        "student_no = excluded.student_no, degree = excluded.degree, "
        "study_year = excluded.study_year, arc_status = excluded.arc_status, "
        "membership = excluded.membership, underage = excluded.underage")};

    bindText(guard.statement, 1, player.id());
    bindText(guard.statement, 2, player.name());
    bindText(guard.statement, 3, player.email());
    bindText(guard.statement, 4, player.studentNumber());
    bindText(guard.statement, 5, player.degree());
    sqlite3_bind_int(guard.statement, 6, player.studyYear());
    sqlite3_bind_text(guard.statement, 7, domain::arcStatusName(player.arcStatus()), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(guard.statement, 8, domain::membershipTypeName(player.membership()), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(guard.statement, 9, player.isUnderage() ? 1 : 0);

    if (sqlite3_step(guard.statement) != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: insert player failed");
    }
}

void SqliteStore::loadPlayers(PlayerDirectory& directory) {
    StatementGuard guard{prepare(
        "SELECT id, name, email, student_no, degree, study_year, arc_status, "
        "membership, underage, created_at FROM player ORDER BY id")};

    while (sqlite3_step(guard.statement) == SQLITE_ROW) {
        sqlite3_stmt* row = guard.statement;
        Player player(columnText(row, 0), columnText(row, 1));
        player.setEmail(columnText(row, 2));
        player.setStudentNumber(columnText(row, 3));
        player.setDegree(columnText(row, 4));
        player.setStudyYear(sqlite3_column_int(row, 5));
        player.setArcStatus(domain::arcStatusFromName(columnText(row, 6)));
        player.setMembership(domain::membershipTypeFromName(columnText(row, 7)));
        player.setUnderage(sqlite3_column_int(row, 8) != 0);
        player.setCreatedAt(columnText(row, 9));
        directory.upsert(std::move(player));
    }
}

// ---- attendance ----

domain::AttendanceRecord SqliteStore::recordAttendance(const std::string& playerId) {
    domain::AttendanceRecord record{playerId, util::nowUtc()};
    StatementGuard guard{prepare("INSERT INTO attendance(player_id, scanned_at) VALUES(?, ?)")};
    bindText(guard.statement, 1, record.playerId);
    bindText(guard.statement, 2, record.time);
    if (sqlite3_step(guard.statement) != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: insert attendance failed");
    }
    return record;
}

void SqliteStore::loadAttendance(domain::Attendance& attendance) {
    StatementGuard guard{prepare("SELECT player_id, scanned_at FROM attendance ORDER BY id")};
    while (sqlite3_step(guard.statement) == SQLITE_ROW) {
        attendance.add(domain::AttendanceRecord{columnText(guard.statement, 0),
                                                columnText(guard.statement, 1)});
    }
}

// ---- food catalog ----

long long SqliteStore::saveProduct(const domain::Product& product) {
    if (product.id == 0) {
        StatementGuard guard{
            prepare("INSERT INTO product(name, price_cents, image) VALUES(?, ?, ?)")};
        bindText(guard.statement, 1, product.name);
        sqlite3_bind_int64(guard.statement, 2, product.price.cents());
        bindText(guard.statement, 3, product.image);
        if (sqlite3_step(guard.statement) != SQLITE_DONE) {
            throw std::runtime_error("SqliteStore: insert product failed");
        }
        return sqlite3_last_insert_rowid(db_);
    }

    StatementGuard guard{
        prepare("UPDATE product SET name = ?, price_cents = ?, image = ? WHERE id = ?")};
    bindText(guard.statement, 1, product.name);
    sqlite3_bind_int64(guard.statement, 2, product.price.cents());
    bindText(guard.statement, 3, product.image);
    sqlite3_bind_int64(guard.statement, 4, product.id);
    if (sqlite3_step(guard.statement) != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: update product failed");
    }
    return product.id;
}

void SqliteStore::deleteProduct(long long id) {
    exec("BEGIN");
    try {
        {
            StatementGuard guard{prepare("DELETE FROM food_slot WHERE product_id = ?")};
            sqlite3_bind_int64(guard.statement, 1, id);
            sqlite3_step(guard.statement);
        }
        {
            StatementGuard guard{prepare("DELETE FROM product WHERE id = ?")};
            sqlite3_bind_int64(guard.statement, 1, id);
            if (sqlite3_step(guard.statement) != SQLITE_DONE) {
                throw std::runtime_error("SqliteStore: delete product failed");
            }
        }
        exec("COMMIT");
    } catch (...) {
        exec("ROLLBACK");
        throw;
    }
}

void SqliteStore::saveFoodSlot(int slot, long long productId) {
    if (productId == 0) {
        StatementGuard guard{prepare("DELETE FROM food_slot WHERE slot = ?")};
        sqlite3_bind_int(guard.statement, 1, slot);
        sqlite3_step(guard.statement);
        return;
    }
    StatementGuard guard{prepare(
        "INSERT INTO food_slot(slot, product_id) VALUES(?, ?) "
        "ON CONFLICT(slot) DO UPDATE SET product_id = excluded.product_id")};
    sqlite3_bind_int(guard.statement, 1, slot);
    sqlite3_bind_int64(guard.statement, 2, productId);
    if (sqlite3_step(guard.statement) != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: save food slot failed");
    }
}

void SqliteStore::loadCatalog(domain::ProductCatalog& catalog) {
    {
        StatementGuard guard{prepare("SELECT id, name, price_cents, image FROM product")};
        while (sqlite3_step(guard.statement) == SQLITE_ROW) {
            domain::Product product;
            product.id = sqlite3_column_int64(guard.statement, 0);
            product.name = columnText(guard.statement, 1);
            product.price = Money::fromCents(sqlite3_column_int64(guard.statement, 2));
            product.image = columnText(guard.statement, 3);
            catalog.upsert(std::move(product));
        }
    }
    StatementGuard guard{prepare("SELECT slot, product_id FROM food_slot")};
    while (sqlite3_step(guard.statement) == SQLITE_ROW) {
        catalog.setSlot(sqlite3_column_int(guard.statement, 0),
                        sqlite3_column_int64(guard.statement, 1));
    }
}

// ---- settings ----

void SqliteStore::setSetting(const std::string& key, const std::string& value) {
    StatementGuard guard{prepare(
        "INSERT INTO app_setting(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value")};
    bindText(guard.statement, 1, key);
    bindText(guard.statement, 2, value);
    if (sqlite3_step(guard.statement) != SQLITE_DONE) {
        throw std::runtime_error("SqliteStore: set setting failed");
    }
}

std::optional<std::string> SqliteStore::getSetting(const std::string& key) {
    StatementGuard guard{prepare("SELECT value FROM app_setting WHERE key = ?")};
    bindText(guard.statement, 1, key);
    if (sqlite3_step(guard.statement) == SQLITE_ROW) {
        return columnText(guard.statement, 0);
    }
    return std::nullopt;
}

// ---- maintenance ----

void SqliteStore::backupTo(const std::string& path) {
    sqlite3* target = nullptr;
    if (sqlite3_open(path.c_str(), &target) != SQLITE_OK) {
        const std::string message = target ? sqlite3_errmsg(target) : "open failed";
        sqlite3_close(target);
        throw std::runtime_error("Backup: cannot open '" + path + "': " + message);
    }

    sqlite3_backup* backup = sqlite3_backup_init(target, "main", db_, "main");
    if (!backup) {
        const std::string message = sqlite3_errmsg(target);
        sqlite3_close(target);
        throw std::runtime_error("Backup failed: " + message);
    }
    sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);

    const int rc = sqlite3_errcode(target);
    const std::string message = sqlite3_errmsg(target);
    sqlite3_close(target);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Backup failed: " + message);
    }
}

} // namespace pokersoc::persistence
