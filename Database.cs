using Microsoft.Data.Sqlite;
using System;
using System.Data;
using System.IO;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;

namespace pokersoc_connect
{
    public static class Database
    {
        public static SqliteConnection? Conn { get; private set; }

        public static void Open(string path)
        {
            Close();
            var cs = new SqliteConnectionStringBuilder { DataSource = path }.ToString();
            Conn = new SqliteConnection(cs);
            Conn.Open();

            // Durable journaling + crash safety by default
            using (var cmd = Conn.CreateCommand())
            {
                cmd.CommandText = "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;";
                cmd.ExecuteNonQuery();
            }
        }

        public static void Close()
        {
            Conn?.Dispose();
            Conn = null;
        }

        /// Find an embedded resource by file name suffix (case-insensitive).
        public static string? FindResourceName(Assembly asm, string fileName)
        {
            var want = fileName.ToLowerInvariant();
            foreach (var name in asm.GetManifestResourceNames())
                if (name.ToLowerInvariant().EndsWith(want)) return name;
            return null;
        }

        public static void EnsureSchemaFromResource(Assembly asm, string resourceName)
        {
            using var stream = asm.GetManifestResourceStream(resourceName)
              ?? throw new InvalidOperationException($"Embedded resource not found: {resourceName}");
            using var reader = new StreamReader(stream, Encoding.UTF8, detectEncodingFromByteOrderMarks: true);
            var sql = reader.ReadToEnd();
            ApplySqlStatements(sql);
        }

        /// Split and apply multiple SQL statements safely (schema file).
        private static void ApplySqlStatements(string sql)
        {
            if (Conn == null) throw new InvalidOperationException("Database not open.");
            sql = sql.Replace("\r\n", "\n");

            var parts = Regex.Split(sql, @";\s*(\n|$)", RegexOptions.Multiline);
            foreach (var raw in parts)
            {
                var stmt = raw.Trim();
                if (string.IsNullOrEmpty(stmt)) continue;
                if (stmt.StartsWith("--") || stmt.StartsWith("/*")) continue;

                using var cmd = Conn.CreateCommand();
                cmd.CommandText = stmt + ";";
                try { cmd.ExecuteNonQuery(); }
                catch (Exception ex)
                {
                    var preview = stmt.Length > 200 ? stmt[..200] + "..." : stmt;
                    throw new InvalidOperationException($"Schema statement failed:\n{preview}\n\n{ex.Message}", ex);
                }
            }
        }

        public static bool IsFreshDb()
        {
            using var cmd = Conn!.CreateCommand();
            cmd.CommandText = "SELECT COUNT(*) FROM sessions";
            var count = (long)(cmd.ExecuteScalar() ?? 0L);
            return count == 0;
        }

        public static void SeedDefaultSessionIfEmpty()
        {
            if (!IsFreshDb()) return;
            using var cmd = Conn!.CreateCommand();
            cmd.CommandText = "INSERT INTO sessions(name, date, currency, open_time) VALUES ($n, date('now'), 'AUD', time('now'))";
            cmd.Parameters.AddWithValue("$n", "New Session");
            cmd.ExecuteNonQuery();
        }

        // ---------------- Transactions & helpers ----------------

        public static void InTransaction(Action<SqliteTransaction> body)
        {
            using var tx = Conn!.BeginTransaction();
            try { body(tx); tx.Commit(); }
            catch { try { tx.Rollback(); } catch { } throw; }
        }

        // With-transaction overloads
        public static int Exec(string sql, SqliteTransaction? tx, params (string, object?)[] p)
        {
            using var cmd = Conn!.CreateCommand();
            cmd.Transaction = tx;
            cmd.CommandText = sql;
            foreach (var (k, v) in p) cmd.Parameters.AddWithValue(k, v ?? DBNull.Value);
            return cmd.ExecuteNonQuery();
        }

        public static long ScalarLong(string sql, SqliteTransaction? tx, params (string, object?)[] p)
        {
            using var cmd = Conn!.CreateCommand();
            cmd.Transaction = tx;
            cmd.CommandText = sql;
            foreach (var (k, v) in p) cmd.Parameters.AddWithValue(k, v ?? DBNull.Value);
            return Convert.ToInt64(cmd.ExecuteScalar() ?? 0);
        }

        // Original convenience overloads (no transaction)
        public static int Exec(string sql, params (string, object?)[] p) => Exec(sql, null, p);
        public static long ScalarLong(string sql, params (string, object?)[] p) => ScalarLong(sql, null, p);

        public static DataTable Query(string sql, params (string, object?)[] p)
        {
            using var cmd = Conn!.CreateCommand();
            cmd.CommandText = sql;
            foreach (var (k, v) in p) cmd.Parameters.AddWithValue(k, v ?? DBNull.Value);
            using var rdr = cmd.ExecuteReader();
            var dt = new DataTable();
            dt.Load(rdr);
            return dt;
        }
    public static bool TableExists(string name)
{
    using var cmd = Conn!.CreateCommand();
    cmd.CommandText = "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=$n";
    cmd.Parameters.AddWithValue("$n", name);
    return Convert.ToInt64(cmd.ExecuteScalar() ?? 0) > 0;
}

    public static void EnsureCashboxTables()
    {
        // float
        if (!TableExists("cashbox_float"))
        {
            Exec(@"
    CREATE TABLE IF NOT EXISTS cashbox_float (
    session_id   INTEGER NOT NULL REFERENCES sessions(session_id) ON DELETE CASCADE,
    denom_cents  INTEGER NOT NULL,
    qty          INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (session_id, denom_cents)
    )");
        }

        // movements
        if (!TableExists("cashbox_movements"))
        {
            Exec(@"
    CREATE TABLE IF NOT EXISTS cashbox_movements (
    move_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id   INTEGER NOT NULL REFERENCES sessions(session_id) ON DELETE CASCADE,
    time         TEXT DEFAULT CURRENT_TIMESTAMP,
    denom_cents  INTEGER NOT NULL,
    delta_qty    INTEGER NOT NULL,
    reason       TEXT NOT NULL,
    player_id    INTEGER,
    tx_id        INTEGER,
    notes        TEXT
    )");
        }
    }


  }
}
