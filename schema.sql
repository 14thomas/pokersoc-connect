PRAGMA foreign_keys = ON;

-- Players: scanned barcode/member number is the primary key
CREATE TABLE IF NOT EXISTS players (
  player_id    TEXT PRIMARY KEY,          -- barcode you scan
  display_name TEXT NOT NULL,
  first_name   TEXT,
  last_name    TEXT,
  phone        TEXT,
  notes        TEXT,
  status       TEXT DEFAULT 'Active',
  created_at   TEXT DEFAULT CURRENT_TIMESTAMP
);

-- Transactions: only cash_amt is stored (chips == cash)
CREATE TABLE IF NOT EXISTS transactions (
  tx_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  time      TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  type      TEXT NOT NULL CHECK (type IN ('BUYIN','CASHOUT')),
  player_id TEXT NOT NULL REFERENCES players(player_id) ON DELETE CASCADE,
  cash_amt  REAL NOT NULL,
  method    TEXT,
  staff     TEXT,
  notes     TEXT
);

-- Starting float for each denomination (per DB/session)
CREATE TABLE IF NOT EXISTS cashbox_float (
  denom_cents INTEGER PRIMARY KEY,        -- 5,10,20,50,...,10000
  qty         INTEGER NOT NULL DEFAULT 0
);

-- Movements of cash into/out of the cashbox by denomination
CREATE TABLE IF NOT EXISTS cashbox_movements (
  move_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  time        TEXT DEFAULT CURRENT_TIMESTAMP,
  denom_cents INTEGER NOT NULL,
  delta_qty   INTEGER NOT NULL,           -- + in, - out
  reason      TEXT NOT NULL,              -- 'BUYIN','CASHOUT','ADJUST','FLOAT_ADD','LOST_CHIP'
  player_id   TEXT,                       -- optional
  tx_id       INTEGER,                    -- optional link to transactions(tx_id)
  notes       TEXT
);

-- Tips from lost chips
CREATE TABLE IF NOT EXISTS tips (
  tip_id      INTEGER PRIMARY KEY AUTOINCREMENT,
  time        TEXT DEFAULT CURRENT_TIMESTAMP,
  denom_cents INTEGER NOT NULL,
  qty         INTEGER NOT NULL,
  notes       TEXT
);

-- Activity log for tracking all system activities
CREATE TABLE IF NOT EXISTS activity_log (
  activity_id   INTEGER PRIMARY KEY AUTOINCREMENT,
  activity_key  TEXT NOT NULL UNIQUE,
  activity_type TEXT NOT NULL,
  activity_kind TEXT NOT NULL,
  method        TEXT,
  staff         TEXT,
  player_id     TEXT,
  tx_id         INTEGER,
  batch_id      TEXT,
  amount_cents  INTEGER,
  notes         TEXT,
  time          TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_tx_player_time ON transactions(player_id, time);
CREATE INDEX IF NOT EXISTS idx_moves_denom_time ON cashbox_movements(denom_cents, time);
CREATE INDEX IF NOT EXISTS idx_tips_denom_time ON tips(denom_cents, time);
CREATE INDEX IF NOT EXISTS idx_activity_type_time ON activity_log(activity_type, time);
