using Microsoft.Data.Sqlite;
using System;
using System.Data;

namespace pokersoc_connect
{
  public static class Database
  {
    public static SqliteConnection? Conn { get; private set; }

    public static void Open(string dbPath)
    {
      Close();
      Conn = new SqliteConnection($"Data Source={dbPath};Cache=Shared");
      Conn.Open();
      ApplySchema();
    }

    public static void Close()
    {
      try { Conn?.Close(); } catch { }
      Conn = null;
    }

    private static void ApplySchema()
    {
      // New schema:
      // - players.player_id is TEXT PRIMARY KEY (we store the scanned ID)
      // - NO session_id columns anywhere
      // - cashbox is aggregate of: optional baseline (cashbox_float) + movements (cashbox_movements)
      var sql = @"
CREATE TABLE IF NOT EXISTS players(
  player_id    TEXT PRIMARY KEY,          -- the scanned barcode/id
  first_name   TEXT,
  last_name    TEXT,
  display_name TEXT
);

CREATE TABLE IF NOT EXISTS transactions(
  tx_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  time      TEXT NOT NULL DEFAULT (datetime('now')),
  type      TEXT NOT NULL CHECK(type IN ('BUYIN','CASHOUT')),
  player_id TEXT NOT NULL,
  cash_amt  REAL NOT NULL,
  chips_amt REAL NOT NULL,
  method    TEXT,
  staff     TEXT,
  FOREIGN KEY(player_id) REFERENCES players(player_id)
);

CREATE TABLE IF NOT EXISTS cashbox_float(
  denom_cents INTEGER PRIMARY KEY,
  qty         INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS cashbox_movements(
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  time        TEXT NOT NULL DEFAULT (datetime('now')),
  denom_cents INTEGER NOT NULL,
  delta_qty   INTEGER NOT NULL,
  reason      TEXT NOT NULL,             -- BUYIN, CASHOUT, FLOAT_ADD, etc
  player_id   TEXT NULL,                  -- optional link
  tx_id       INTEGER NULL               -- optional link
);

CREATE INDEX IF NOT EXISTS idx_tx_time ON transactions(time DESC);
CREATE INDEX IF NOT EXISTS idx_mov_denom ON cashbox_movements(denom_cents);
";
      Exec(sql);
    }

    // ---- helpers ----
    public static void Exec(string sql, params (string, object?)[] args)
    {
      if (Conn is null) throw new InvalidOperationException("DB not open.");
      using var cmd = Conn.CreateCommand();
      cmd.CommandText = sql;
      foreach (var (n, v) in args) cmd.Parameters.AddWithValue(n, v ?? DBNull.Value);
      cmd.ExecuteNonQuery();
    }

    public static void Exec(string sql, SqliteTransaction? tx, params (string, object?)[] args)
    {
      if (Conn is null) throw new InvalidOperationException("DB not open.");
      using var cmd = Conn.CreateCommand();
      cmd.Transaction = tx;
      cmd.CommandText = sql;
      foreach (var (n, v) in args) cmd.Parameters.AddWithValue(n, v ?? DBNull.Value);
      cmd.ExecuteNonQuery();
    }

    public static DataTable Query(string sql, params (string, object?)[] args)
    {
      if (Conn is null) throw new InvalidOperationException("DB not open.");
      using var cmd = Conn.CreateCommand();
      cmd.CommandText = sql;
      foreach (var (n, v) in args) cmd.Parameters.AddWithValue(n, v ?? DBNull.Value);
      using var rdr = cmd.ExecuteReader();
      var dt = new DataTable();
      dt.Load(rdr);
      return dt;
    }

    public static DataTable Query(string sql, SqliteTransaction? tx, params (string, object?)[] args)
    {
      if (Conn is null) throw new InvalidOperationException("DB not open.");
      using var cmd = Conn.CreateCommand();
      cmd.Transaction = tx;
      cmd.CommandText = sql;
      foreach (var (n, v) in args) cmd.Parameters.AddWithValue(n, v ?? DBNull.Value);
      using var rdr = cmd.ExecuteReader();
      var dt = new DataTable();
      dt.Load(rdr);
      return dt;
    }

    public static long ScalarLong(string sql, params (string, object?)[] args)
    {
      if (Conn is null) throw new InvalidOperationException("DB not open.");
      using var cmd = Conn.CreateCommand();
      cmd.CommandText = sql;
      foreach (var (n, v) in args) cmd.Parameters.AddWithValue(n, v ?? DBNull.Value);
      var o = cmd.ExecuteScalar();
      if (o is null || o is DBNull) return 0;
      return Convert.ToInt64(o);
    }

    public static long ScalarLong(string sql, SqliteTransaction? tx, params (string, object?)[] args)
    {
      if (Conn is null) throw new InvalidOperationException("DB not open.");
      using var cmd = Conn.CreateCommand();
      cmd.Transaction = tx;
      cmd.CommandText = sql;
      foreach (var (n, v) in args) cmd.Parameters.AddWithValue(n, v ?? DBNull.Value);
      var o = cmd.ExecuteScalar();
      if (o is null || o is DBNull) return 0;
      return Convert.ToInt64(o);
    }

    public static void InTransaction(Action<SqliteTransaction> action)
    {
      if (Conn is null) throw new InvalidOperationException("DB not open.");
      using var tx = Conn.BeginTransaction();
      action(tx);
      tx.Commit();
    }
  }
}
