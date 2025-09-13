PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS players (
  player_id    INTEGER PRIMARY KEY AUTOINCREMENT,
  member_no    TEXT UNIQUE,
  first_name   TEXT NOT NULL,
  last_name    TEXT NOT NULL,
  display_name TEXT,
  phone        TEXT,
  notes        TEXT,
  status       TEXT DEFAULT 'Active',
  created_at   TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS sessions (
  session_id  INTEGER PRIMARY KEY AUTOINCREMENT,
  name        TEXT NOT NULL,
  date        TEXT NOT NULL,
  venue       TEXT,
  game        TEXT,
  currency    TEXT DEFAULT 'AUD',
  open_time   TEXT,
  close_time  TEXT,
  notes       TEXT
);

CREATE TABLE IF NOT EXISTS transactions (
  tx_id       INTEGER PRIMARY KEY AUTOINCREMENT,
  session_id  INTEGER NOT NULL REFERENCES sessions(session_id) ON DELETE CASCADE,
  table_id    INTEGER,
  player_id   INTEGER NOT NULL REFERENCES players(player_id) ON DELETE CASCADE,
  type        TEXT NOT NULL CHECK (type IN ('BUYIN','CASHOUT')),
  time        TEXT DEFAULT CURRENT_TIMESTAMP,
  cash_amt    REAL NOT NULL,
  chips_amt   REAL NOT NULL,
  method      TEXT,
  staff       TEXT,
  notes       TEXT
);

CREATE VIEW IF NOT EXISTS player_session_balances AS
SELECT
  p.player_id,
  s.session_id,
  p.display_name,
  s.name AS session_name,
  SUM(CASE WHEN t.type='BUYIN'  THEN  t.chips_amt ELSE -t.chips_amt END) AS chips_delta,
  SUM(CASE WHEN t.type='BUYIN'  THEN  t.cash_amt  ELSE -t.cash_amt  END) AS cash_delta
FROM players p
JOIN transactions t ON t.player_id = p.player_id
JOIN sessions s     ON s.session_id = t.session_id
GROUP BY p.player_id, s.session_id;
