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
      // --- Core schema (one DB == one session) ---
      var sql = @"
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS players (
  player_id    TEXT PRIMARY KEY,          -- scanned barcode/id
  display_name TEXT NOT NULL,
  first_name   TEXT,
  last_name    TEXT,
  phone        TEXT,
  notes        TEXT,
  status       TEXT DEFAULT 'Active',
  created_at   TEXT DEFAULT CURRENT_TIMESTAMP
);

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

CREATE TABLE IF NOT EXISTS cashbox_float (
  denom_cents INTEGER PRIMARY KEY,        -- 5,10,20,50,100,200,500,2500,10000 (we only use the ones you keep in UI)
  qty         INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS cashbox_movements (
  move_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  time        TEXT DEFAULT CURRENT_TIMESTAMP,
  denom_cents INTEGER NOT NULL,
  delta_qty   INTEGER NOT NULL,           -- + in, - out (cash only)
  reason      TEXT NOT NULL,              -- 'BUYIN','CASHOUT','ADJUST','FLOAT_ADD'
  player_id   TEXT,                       -- optional link
  tx_id       INTEGER,                    -- optional link to transactions(tx_id)
  notes       TEXT
);

CREATE INDEX IF NOT EXISTS idx_tx_player_time ON transactions(player_id, time);
CREATE INDEX IF NOT EXISTS idx_moves_denom_time ON cashbox_movements(denom_cents, time);
";
      Exec(sql);

      // Grouping id for a multi-denomination FLOAT_ADD
      try { Exec("ALTER TABLE cashbox_movements ADD COLUMN batch_id TEXT"); } catch { }

      // NEW: per-transaction CHIP breakdown for CASHOUT (and optionally BUYIN, if you want later)
      try
      {
        Exec(@"
CREATE TABLE IF NOT EXISTS tx_chips(
  tx_id       INTEGER NOT NULL,
  denom_cents INTEGER NOT NULL,   -- chip/ plaque denominations (5,25,100,200,500,2500,10000)
  qty         INTEGER NOT NULL,   -- positive = chips turned in (for CASHOUT)
  PRIMARY KEY(tx_id, denom_cents),
  FOREIGN KEY(tx_id) REFERENCES transactions(tx_id) ON DELETE CASCADE
);");
      }
      catch { /* already there */ }
    }

    // ----------------- Helpers -----------------
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

    // Manual DataTable loader to avoid spurious Unique/Constraint issues with UNIONs
    public static DataTable Query(string sql, params (string, object?)[] args)
    {
      if (Conn is null) throw new InvalidOperationException("DB not open.");
      using var cmd = Conn.CreateCommand();
      cmd.CommandText = sql;
      foreach (var (n, v) in args) cmd.Parameters.AddWithValue(n, v ?? DBNull.Value);

      using var rdr = cmd.ExecuteReader(System.Data.CommandBehavior.SequentialAccess);

      var dt = new DataTable();
      int fieldCount = rdr.FieldCount;
      for (int i = 0; i < fieldCount; i++)
        dt.Columns.Add(rdr.GetName(i), typeof(object));

      while (rdr.Read())
      {
        var row = dt.NewRow();
        for (int i = 0; i < fieldCount; i++)
          row[i] = rdr.IsDBNull(i) ? DBNull.Value : rdr.GetValue(i);
        dt.Rows.Add(row);
      }
      return dt;
    }

    public static DataTable Query(string sql, SqliteTransaction? tx, params (string, object?)[] args)
    {
      if (Conn is null) throw new InvalidOperationException("DB not open.");
      using var cmd = Conn.CreateCommand();
      cmd.Transaction = tx;
      cmd.CommandText = sql;
      foreach (var (n, v) in args) cmd.Parameters.AddWithValue(n, v ?? DBNull.Value);

      using var rdr = cmd.ExecuteReader(System.Data.CommandBehavior.SequentialAccess);

      var dt = new DataTable();
      int fieldCount = rdr.FieldCount;
      for (int i = 0; i < fieldCount; i++)
        dt.Columns.Add(rdr.GetName(i), typeof(object));

      while (rdr.Read())
      {
        var row = dt.NewRow();
        for (int i = 0; i < fieldCount; i++)
          row[i] = rdr.IsDBNull(i) ? DBNull.Value : rdr.GetValue(i);
        dt.Rows.Add(row);
      }
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
