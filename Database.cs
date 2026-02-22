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
  player_id      TEXT PRIMARY KEY,          -- scanned barcode/id (or typed when no card present)
  display_name   TEXT NOT NULL DEFAULT 'New Player',
  email          TEXT DEFAULT 'None',
  student_number TEXT DEFAULT 'None',
  degree         TEXT DEFAULT 'None',
  study_year     TEXT DEFAULT 'None',
  arc_member     TEXT DEFAULT 'None',
  created_at     TEXT DEFAULT CURRENT_TIMESTAMP
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

      // Migrate player table to new schema (add new columns if they don't exist)
      try { Exec("ALTER TABLE players ADD COLUMN email TEXT DEFAULT 'None'"); } catch { }
      try { Exec("ALTER TABLE players ADD COLUMN student_number TEXT DEFAULT 'None'"); } catch { }
      try { Exec("ALTER TABLE players ADD COLUMN degree TEXT DEFAULT 'None'"); } catch { }
      try { Exec("ALTER TABLE players ADD COLUMN study_year TEXT DEFAULT 'None'"); } catch { }
      try { Exec("ALTER TABLE players ADD COLUMN arc_member TEXT DEFAULT 'None'"); } catch { }
      try { Exec("ALTER TABLE players ADD COLUMN is_underage INTEGER DEFAULT 0"); } catch { }

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
  product_id   INTEGER REFERENCES products(product_id) ON DELETE RESTRICT,
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
      
      // Make product_id nullable in sale_items (for food sales during cashout that may not have product in catalog)
      try
      {
        // SQLite doesn't support ALTER COLUMN, so we need to recreate the table
        Exec(@"
          CREATE TABLE IF NOT EXISTS sale_items_new(
            sale_item_id INTEGER PRIMARY KEY AUTOINCREMENT,
            sale_id      INTEGER NOT NULL REFERENCES sales(sale_id) ON DELETE CASCADE,
            product_id   INTEGER REFERENCES products(product_id) ON DELETE RESTRICT,
            qty          INTEGER NOT NULL,
            unit_price   REAL NOT NULL
          );
        ");
        Exec("INSERT INTO sale_items_new SELECT * FROM sale_items");
        Exec("DROP TABLE sale_items");
        Exec("ALTER TABLE sale_items_new RENAME TO sale_items");
      }
      catch { /* migration already done or table structure matches */ }

      // ----- NEW: Tips from Lost Chips -----
      Exec(@"
CREATE TABLE IF NOT EXISTS tips (
  tip_id      INTEGER PRIMARY KEY AUTOINCREMENT,
  time        TEXT DEFAULT CURRENT_TIMESTAMP,
  denom_cents INTEGER NOT NULL,
  qty         INTEGER NOT NULL,
  player_id   TEXT,
  tx_id       INTEGER,
  notes       TEXT
);
");

      // ----- Player Attendance Tracking -----
      Exec(@"
CREATE TABLE IF NOT EXISTS player_attendance (
  attendance_id INTEGER PRIMARY KEY AUTOINCREMENT,
  player_id     TEXT NOT NULL,
  scan_time     TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (player_id) REFERENCES players(player_id) ON DELETE CASCADE
);
");
      Exec("CREATE INDEX IF NOT EXISTS idx_attendance_player ON player_attendance(player_id)");
      Exec("CREATE INDEX IF NOT EXISTS idx_attendance_time ON player_attendance(scan_time)");
      try { Exec("CREATE INDEX IF NOT EXISTS idx_tips_denom_time ON tips(denom_cents, time)"); } catch { }

      // Migrate tips table to add player_id and tx_id if they don't exist
      try { Exec("ALTER TABLE tips ADD COLUMN player_id TEXT"); } catch { }
      try { Exec("ALTER TABLE tips ADD COLUMN tx_id INTEGER"); } catch { }
      try { Exec("ALTER TABLE tips ADD COLUMN batch_id TEXT"); } catch { }

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

      // ----- NEW: App Settings (including admin password) -----
      Exec(@"
CREATE TABLE IF NOT EXISTS app_settings (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL
);
");
      // Set default admin password if not exists
      Exec("INSERT OR IGNORE INTO app_settings(key, value) VALUES ('admin_password', '1234')");
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

    // ----------------- Player Attendance -----------------
    
    // Log player attendance (when ID is scanned/entered)
    public static void LogPlayerAttendance(string playerId)
    {
      Exec("INSERT INTO player_attendance(player_id) VALUES ($id)", ("$id", playerId));
    }
    
    // ----------------- Session players export -----------------
    
    // Export players who attended the current session (scanned their ID)
    public static void ExportSessionPlayers(string filePath)
    {
      // Get all unique player_ids from attendance log
      var sessionPlayers = Query(@"
        SELECT DISTINCT p.player_id, p.display_name, p.email, p.student_number, p.degree, p.study_year, p.arc_member,
               MIN(a.scan_time) as first_scan
        FROM players p
        INNER JOIN player_attendance a ON p.player_id = a.player_id
        GROUP BY p.player_id
        ORDER BY first_scan
      ");
      
      var lines = new List<string>();
      lines.Add("Timestamp, Full Name, Email Address, ARC member, ZID (N/A if non-UNSW), Player ID");
      
      foreach (System.Data.DataRow row in sessionPlayers.Rows)
      {
        var firstScan = row["first_scan"]?.ToString() ?? "";
        var name = row["display_name"]?.ToString() ?? "New Player";
        var email = row["email"]?.ToString() ?? "None";
        var arcMember = row["arc_member"]?.ToString() ?? "None";
        var studentNum = row["student_number"]?.ToString() ?? "None";
        var playerId = row["player_id"]?.ToString() ?? "None";
        
        
        // Simple CSV escaping - quote fields that might contain commas
        lines.Add($"\"{firstScan}\",\"{name}\",\"{email}\",\"{arcMember}\",\"{studentNum}\",\"{playerId}\"");
      }
      
      System.IO.File.WriteAllLines(filePath, lines);
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

    // ----------------- Player Database Management -----------------

    public static void ExportPlayers(string filePath)
    {
      var players = Query("SELECT player_id, display_name, email, student_number, degree, study_year, arc_member, COALESCE(is_underage, 0) as is_underage, created_at FROM players ORDER BY player_id");
      
      using var writer = new StreamWriter(filePath);
      // Write CSV header
      writer.WriteLine("player_id,display_name,email,student_number,degree,study_year,arc_member,is_underage,created_at");
      
      foreach (DataRow row in players.Rows)
      {
        var playerId = CsvEscape(row["player_id"]?.ToString() ?? "");
        var displayName = CsvEscape(row["display_name"]?.ToString() ?? "New Player");
        var email = CsvEscape(row["email"]?.ToString() ?? "None");
        var studentNumber = CsvEscape(row["student_number"]?.ToString() ?? "None");
        var degree = CsvEscape(row["degree"]?.ToString() ?? "None");
        var studyYear = CsvEscape(row["study_year"]?.ToString() ?? "None");
        var arcMember = CsvEscape(row["arc_member"]?.ToString() ?? "None");
        var isUnderage = row["is_underage"]?.ToString() ?? "0";
        var createdAt = CsvEscape(row["created_at"]?.ToString() ?? "");
        
        writer.WriteLine($"{playerId},{displayName},{email},{studentNumber},{degree},{studyYear},{arcMember},{isUnderage},{createdAt}");
      }
    }

    public static void ImportPlayers(string filePath, bool merge = true)
    {
      if (!File.Exists(filePath)) return;

      var lines = File.ReadAllLines(filePath);
      if (lines.Length == 0) return;

      // Skip header row
      for (int i = 1; i < lines.Length; i++)
      {
        var parts = CsvParseLine(lines[i]);
        if (parts.Length < 2) continue;

        var playerId = parts[0].Trim();
        var displayName = parts.Length > 1 ? parts[1].Trim() : "New Player";
        var email = parts.Length > 2 ? parts[2].Trim() : "None";
        var studentNumber = parts.Length > 3 ? parts[3].Trim() : "None";
        var degree = parts.Length > 4 ? parts[4].Trim() : "None";
        var studyYear = parts.Length > 5 ? parts[5].Trim() : "None";
        var arcMember = parts.Length > 6 ? parts[6].Trim() : "None";
        var isUnderageStr = parts.Length > 7 ? parts[7].Trim() : "0";
        int isUnderage = (isUnderageStr == "1" || isUnderageStr.Equals("true", StringComparison.OrdinalIgnoreCase)) ? 1 : 0;

        if (string.IsNullOrWhiteSpace(playerId)) continue;
        if (string.IsNullOrWhiteSpace(displayName)) displayName = "New Player";
        if (string.IsNullOrWhiteSpace(email)) email = "None";
        if (string.IsNullOrWhiteSpace(studentNumber)) studentNumber = "None";
        if (string.IsNullOrWhiteSpace(degree)) degree = "None";
        if (string.IsNullOrWhiteSpace(studyYear)) studyYear = "None";
        if (string.IsNullOrWhiteSpace(arcMember)) arcMember = "None";

        try
        {
          if (merge)
          {
            // Insert or replace (update if exists)
            Exec(@"INSERT OR REPLACE INTO players(player_id, display_name, email, student_number, degree, study_year, arc_member, is_underage) 
                   VALUES ($id, $dn, $em, $sn, $deg, $sy, $arc, $underage)",
                 ("$id", playerId),
                 ("$dn", displayName),
                 ("$em", email),
                 ("$sn", studentNumber),
                 ("$deg", degree),
                 ("$sy", studyYear),
                 ("$arc", arcMember),
                 ("$underage", isUnderage));
          }
          else
          {
            // Only insert if doesn't exist
            Exec(@"INSERT OR IGNORE INTO players(player_id, display_name, email, student_number, degree, study_year, arc_member, is_underage) 
                   VALUES ($id, $dn, $em, $sn, $deg, $sy, $arc, $underage)",
                 ("$id", playerId),
                 ("$dn", displayName),
                 ("$em", email),
                 ("$sn", studentNumber),
                 ("$deg", degree),
                 ("$sy", studyYear),
                 ("$arc", arcMember),
                 ("$underage", isUnderage));
          }
        }
        catch
        {
          // Skip problematic rows
        }
      }
    }

    public static bool IsNewPlayer(string playerId)
    {
      var count = ScalarLong("SELECT COUNT(*) FROM players WHERE player_id = $id", ("$id", playerId));
      return count == 0;
    }

    public static bool IsPlayerUnderage(string playerId)
    {
      var result = ScalarLong("SELECT COALESCE(is_underage, 0) FROM players WHERE player_id = $id", ("$id", playerId));
      return result == 1;
    }

    public static void SetPlayerUnderage(string playerId, bool isUnderage)
    {
      Exec("UPDATE players SET is_underage = $underage WHERE player_id = $id", 
           ("$underage", isUnderage ? 1 : 0), ("$id", playerId));
    }

    // Get player details - returns dictionary with field values (null if player not found)
    public static Dictionary<string, string>? GetPlayerDetails(string playerId)
    {
      var result = Query(
        "SELECT display_name, email, student_number, degree, study_year, arc_member FROM players WHERE player_id = $id",
        ("$id", playerId)
      );
      
      if (result.Rows.Count == 0) return null;
      
      var row = result.Rows[0];
      return new Dictionary<string, string>
      {
        ["display_name"] = row["display_name"]?.ToString() ?? "New Player",
        ["email"] = row["email"]?.ToString() ?? "None",
        ["student_number"] = row["student_number"]?.ToString() ?? "None",
        ["degree"] = row["degree"]?.ToString() ?? "None",
        ["study_year"] = row["study_year"]?.ToString() ?? "None",
        ["arc_member"] = row["arc_member"]?.ToString() ?? "None"
      };
    }

    // Get total buy-ins for a player in the current session (in cents)
    public static long GetPlayerSessionBuyInsCents(string playerId)
    {
      var result = ScalarLong(
        "SELECT COALESCE(SUM(CAST(cash_amt * 100 AS INTEGER)), 0) FROM transactions WHERE player_id = $id AND type = 'BUYIN'",
        ("$id", playerId)
      );
      return result;
    }

    // ----------------- Admin Password -----------------

    public static string GetAdminPassword()
    {
      try
      {
        var result = Query("SELECT value FROM app_settings WHERE key = 'admin_password'");
        if (result.Rows.Count > 0)
        {
          return result.Rows[0]["value"]?.ToString() ?? "1234";
        }
      }
      catch { }
      return "1234"; // Default password
    }

    public static void SetAdminPassword(string password)
    {
      Exec("INSERT OR REPLACE INTO app_settings(key, value) VALUES ('admin_password', $pwd)", ("$pwd", password));
    }

    // ----------------- Transaction Deletion (Undo) -----------------

    public static void DeleteTransaction(long txId, string txType)
    {
      InTransaction(tx =>
      {
        if (txType == "BUYIN")
        {
          // Undo BUYIN: reverse all cashbox movements for this transaction
          // Cash received was added (positive), so we subtract
          // Change given was subtracted (negative), so we add back
          Exec(@"
            INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id, notes)
            SELECT denom_cents, -delta_qty, 'UNDO_BUYIN', player_id, NULL, 'Undo of tx_id ' || $tx
            FROM cashbox_movements WHERE tx_id = $tx", tx, ("$tx", txId));
        }
        else if (txType == "CASHOUT" || txType == "CASHOUT_SALE")
        {
          // Undo CASHOUT: reverse all cashbox movements
          // Cash paid out was subtracted (negative), so we add back
          // Extra cash was added (positive), so we subtract
          Exec(@"
            INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id, notes)
            SELECT denom_cents, -delta_qty, 'UNDO_CASHOUT', player_id, NULL, 'Undo of tx_id ' || $tx
            FROM cashbox_movements WHERE tx_id = $tx", tx, ("$tx", txId));

          // Delete tips associated with this transaction
          Exec("DELETE FROM tips WHERE tx_id = $tx", tx, ("$tx", txId));

          // Delete chips record
          Exec("DELETE FROM tx_chips WHERE tx_id = $tx", tx, ("$tx", txId));

          // Delete any sales associated with this cashout (food sales)
          // First find and delete sale_payments and sale_items, then sales
          var salesResult = Query("SELECT sale_id FROM sales WHERE notes LIKE '%via cashout%' AND player_id = (SELECT player_id FROM transactions WHERE tx_id = $tx)", tx, ("$tx", txId));
          foreach (System.Data.DataRow row in salesResult.Rows)
          {
            var saleId = Convert.ToInt64(row["sale_id"]);
            Exec("DELETE FROM sale_payments WHERE sale_id = $sid", tx, ("$sid", saleId));
            Exec("DELETE FROM sale_items WHERE sale_id = $sid", tx, ("$sid", saleId));
            Exec("DELETE FROM sales WHERE sale_id = $sid", tx, ("$sid", saleId));
          }
        }

        // Delete the activity log entry
        Exec("DELETE FROM activity_log WHERE tx_id = $tx", tx, ("$tx", txId));

        // Delete the transaction itself
        Exec("DELETE FROM transactions WHERE tx_id = $tx", tx, ("$tx", txId));
      });
    }

    public static void DeleteFloatAddition(string batchId)
    {
      InTransaction(tx =>
      {
        // Reverse float addition by adding negative movements
        Exec(@"
          INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id, notes)
          SELECT denom_cents, -delta_qty, 'UNDO_FLOAT', NULL, NULL, 'Undo of batch ' || $batch
          FROM cashbox_movements WHERE batch_id = $batch AND reason = 'FLOAT_ADD'", tx, ("$batch", batchId));

        // Delete activity log entry
        Exec("DELETE FROM activity_log WHERE batch_id = $batch", tx, ("$batch", batchId));
      });
    }

    public static void DeleteLostChips(string batchId)
    {
      InTransaction(tx =>
      {
        // Delete the tips entries linked to this batch (by batch_id or legacy tx_id link)
        Exec("DELETE FROM tips WHERE batch_id = $batch OR tx_id IN (SELECT move_id FROM cashbox_movements WHERE batch_id = $batch)", tx, ("$batch", batchId));

        // Delete the cashbox movements
        Exec("DELETE FROM cashbox_movements WHERE batch_id = $batch AND reason = 'LOST_CHIP'", tx, ("$batch", batchId));

        // Delete activity log entry
        Exec("DELETE FROM activity_log WHERE batch_id = $batch", tx, ("$batch", batchId));
      });
    }

    public static void DeleteTip(string batchId)
    {
      InTransaction(tx =>
      {
        // Delete tips linked to this batch
        Exec("DELETE FROM tips WHERE batch_id = $batch", tx, ("$batch", batchId));

        // Delete cashbox movements (removing +delta rows reduces cashbox total by the tip amount)
        Exec("DELETE FROM cashbox_movements WHERE batch_id = $batch AND reason = 'TIP'", tx, ("$batch", batchId));

        // Delete activity log entry
        Exec("DELETE FROM activity_log WHERE batch_id = $batch", tx, ("$batch", batchId));
      });
    }

    private static string CsvEscape(string value)
    {
      if (string.IsNullOrEmpty(value)) return "";
      if (value.Contains(",") || value.Contains("\"") || value.Contains("\n"))
      {
        return "\"" + value.Replace("\"", "\"\"") + "\"";
      }
      return value;
    }

    private static string[] CsvParseLine(string line)
    {
      var result = new List<string>();
      var current = new System.Text.StringBuilder();
      bool inQuotes = false;

      for (int i = 0; i < line.Length; i++)
      {
        char c = line[i];
        
        if (c == '"')
        {
          if (inQuotes && i + 1 < line.Length && line[i + 1] == '"')
          {
            current.Append('"');
            i++; // Skip next quote
          }
          else
          {
            inQuotes = !inQuotes;
          }
        }
        else if (c == ',' && !inQuotes)
        {
          result.Add(current.ToString());
          current.Clear();
        }
        else
        {
          current.Append(c);
        }
      }
      
      result.Add(current.ToString());
      return result.ToArray();
    }
  }
}
