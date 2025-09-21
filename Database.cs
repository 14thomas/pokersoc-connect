using Microsoft.Data.Sqlite;
using System;
using System.Collections.Generic;
using System.Data;
using System.IO;
using System.Text.Json;

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
      // Base schema you already use (one DB == one session)
      var sql = @"
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS players (
  player_id    TEXT PRIMARY KEY,          -- scanned barcode/id (or typed when no card present)
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
  denom_cents INTEGER PRIMARY KEY,
  qty         INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS cashbox_movements (
  move_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  time        TEXT DEFAULT CURRENT_TIMESTAMP,
  denom_cents INTEGER NOT NULL,
  delta_qty   INTEGER NOT NULL,           -- + in, - out (cash only)
  reason      TEXT NOT NULL,              -- 'BUYIN','CASHOUT','ADJUST','FLOAT_ADD','SALE'
  player_id   TEXT,                       -- optional link
  tx_id       INTEGER,                    -- optional: transactions.tx_id
  notes       TEXT
);
CREATE INDEX IF NOT EXISTS idx_tx_player_time ON transactions(player_id, time);
CREATE INDEX IF NOT EXISTS idx_moves_denom_time ON cashbox_movements(denom_cents, time);
";
      Exec(sql);

      // Grouping id for multi-denomination float add
      try { Exec("ALTER TABLE cashbox_movements ADD COLUMN batch_id TEXT"); } catch { }

      // Store chip/plaques taken on cash-out (and we’ll reuse for sales via sale_id)
      try
      {
        Exec(@"
CREATE TABLE IF NOT EXISTS tx_chips(
  tx_id       INTEGER,
  denom_cents INTEGER NOT NULL,
  qty         INTEGER NOT NULL,   -- + means chips received by house
  PRIMARY KEY(tx_id, denom_cents),
  FOREIGN KEY(tx_id) REFERENCES transactions(tx_id) ON DELETE CASCADE
);");
      } catch { }

      // ----- NEW: Product Catalog -----
      // Simple products without variants
      Exec(@"
CREATE TABLE IF NOT EXISTS products(
  product_id   INTEGER PRIMARY KEY AUTOINCREMENT,
  name         TEXT NOT NULL,
  price        REAL NOT NULL DEFAULT 0,   -- single price per product
  image_path   TEXT                       -- optional file path
);
");

      // ----- NEW: Sales -----
      // sale header + line items + payments (cash/chips)
      Exec(@"
CREATE TABLE IF NOT EXISTS sales(
  sale_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  time        TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  player_id   TEXT NULL,            -- can be null (anonymous)
  staff       TEXT,
  notes       TEXT
);
");
      Exec(@"
CREATE TABLE IF NOT EXISTS sale_items(
  sale_item_id INTEGER PRIMARY KEY AUTOINCREMENT,
  sale_id      INTEGER NOT NULL REFERENCES sales(sale_id) ON DELETE CASCADE,
  product_id   INTEGER NOT NULL REFERENCES products(product_id) ON DELETE RESTRICT,
  qty          INTEGER NOT NULL,
  unit_price   REAL NOT NULL
);
");
      Exec(@"
CREATE TABLE IF NOT EXISTS sale_payments(
  sale_payment_id INTEGER PRIMARY KEY AUTOINCREMENT,
  sale_id         INTEGER NOT NULL REFERENCES sales(sale_id) ON DELETE CASCADE,
  method          TEXT NOT NULL CHECK (method IN ('CASH','CHIP')),
  denom_cents     INTEGER NOT NULL,               -- for CASH: cash denom; for CHIP: chip denom
  qty             INTEGER NOT NULL                -- for CASH: + cash-in; for CHIP: + chips-in
);
");

      // Allow chip rows to link to a sale, so we can reuse chip stock reporting
      try { Exec("ALTER TABLE tx_chips ADD COLUMN sale_id INTEGER"); } catch { /* already */ }
      try { Exec("CREATE INDEX IF NOT EXISTS idx_txchips_sale ON tx_chips(sale_id)"); } catch { }

      // ----- NEW: Tips from Lost Chips -----
      Exec(@"
CREATE TABLE IF NOT EXISTS tips (
  tip_id      INTEGER PRIMARY KEY AUTOINCREMENT,
  time        TEXT DEFAULT CURRENT_TIMESTAMP,
  denom_cents INTEGER NOT NULL,
  qty         INTEGER NOT NULL,
  notes       TEXT
);
");
      try { Exec("CREATE INDEX IF NOT EXISTS idx_tips_denom_time ON tips(denom_cents, time)"); } catch { }

      // ----- NEW: Activity Log -----
      Exec(@"
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
");
      try { Exec("CREATE INDEX IF NOT EXISTS idx_activity_type_time ON activity_log(activity_type, time)"); } catch { }

      // ----- NEW: Food Slot Assignments -----
      Exec(@"
CREATE TABLE IF NOT EXISTS food_slot_assignments (
  slot_id    INTEGER PRIMARY KEY CHECK (slot_id >= 1 AND slot_id <= 6),
  product_id INTEGER REFERENCES products(product_id) ON DELETE SET NULL,
  UNIQUE(slot_id)
);
");
    }

    // ----------------- Public helpers -----------------

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

    // ----------------- Settings export/import -----------------

    // Exports just the reusable catalog (products) to JSON
    public static void ExportSettings(string filePath)
    {
      var products = Query("SELECT product_id,name,price,image_path FROM products");

      var prodList = new List<object>();

      foreach (DataRow p in products.Rows)
      {
        prodList.Add(new
        {
          name = (string)(p["name"] ?? ""),
          price = Convert.ToDouble(p["price"] ?? 0.0),
          image = (string?)(p["image_path"] == DBNull.Value ? null : p["image_path"])
        });
      }

      var root = new { version = 1, products = prodList };
      var json = JsonSerializer.Serialize(root, new JsonSerializerOptions { WriteIndented = true });
      File.WriteAllText(filePath, json);
    }

    // Import settings into current DB; if merge == false, clears existing catalog first
    public static void ImportSettings(string filePath, bool merge = true)
    {
      var json = File.ReadAllText(filePath);
      using var doc = JsonDocument.Parse(json);
      var root = doc.RootElement;

      if (!merge)
      {
        Exec("DELETE FROM products");
      }

      if (root.TryGetProperty("products", out var prods) && prods.ValueKind == JsonValueKind.Array)
      {
        foreach (var p in prods.EnumerateArray())
        {
          string name = p.GetProperty("name").GetString() ?? "";
          double price = p.TryGetProperty("price", out var bp) ? bp.GetDouble() : 0.0;
          string? img = p.TryGetProperty("image", out var ip) && ip.ValueKind != JsonValueKind.Null ? ip.GetString() : null;

          Exec("INSERT INTO products(name,price,image_path) VALUES ($n,$pr,$img)",
               ("$n", name), ("$pr", price), ("$img", (object?)img ?? DBNull.Value));
        }
      }
    }

    // ----------------- Food Slot Management -----------------

    public static void SaveFoodSlotAssignment(int slotId, int? productId)
    {
      if (productId.HasValue)
      {
        Exec("INSERT OR REPLACE INTO food_slot_assignments(slot_id, product_id) VALUES ($slot, $product)",
             ("$slot", slotId), ("$product", productId.Value));
      }
      else
      {
        Exec("DELETE FROM food_slot_assignments WHERE slot_id = $slot", ("$slot", slotId));
      }
    }

    public static Dictionary<int, int> LoadFoodSlotAssignments()
    {
      var assignments = new Dictionary<int, int>();
      
      try
      {
        var result = Query("SELECT slot_id, product_id FROM food_slot_assignments");
        foreach (DataRow row in result.Rows)
        {
          var slotId = Convert.ToInt32(row["slot_id"]);
          var productId = Convert.ToInt32(row["product_id"]);
          assignments[slotId] = productId;
        }
      }
      catch
      {
        // Return empty dictionary if table doesn't exist or query fails
      }

      return assignments;
    }
  }
}
