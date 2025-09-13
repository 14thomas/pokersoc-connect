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
    }

    public static void Close()
    {
      Conn?.Dispose();
      Conn = null;
    }

    /// <summary>
    /// Try to find an embedded resource by file name suffix (case-insensitive).
    /// Example: "schema.sql" -> "pokersoc_connect.schema.sql"
    /// </summary>
    public static string? FindResourceName(Assembly asm, string fileName)
    {
      var want = fileName.ToLowerInvariant();
      foreach (var name in asm.GetManifestResourceNames())
      {
        if (name.ToLowerInvariant().EndsWith(want))
          return name;
      }
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

    /// <summary>
    /// Apply multiple SQL statements safely, one by one.
    /// Splits on ';' that end a statement (very simple splitter).
    /// </summary>
    private static void ApplySqlStatements(string sql)
    {
      if (Conn == null) throw new InvalidOperationException("Database not open.");
      // Normalize line endings
      sql = sql.Replace("\r\n", "\n");

      // Split on semicolons at end of line/statement.
      // This is a simple splitter—adequate for schema files without tricky strings/procs.
      var parts = Regex.Split(sql, @";\s*(\n|$)", RegexOptions.Multiline);
      foreach (var raw in parts)
      {
        var stmt = raw.Trim();
        if (string.IsNullOrEmpty(stmt)) continue;
        // Ignore pure comments
        if (stmt.StartsWith("--") || stmt.StartsWith("/*")) continue;

        using var cmd = Conn.CreateCommand();
        cmd.CommandText = stmt + ";";
        try
        {
          cmd.ExecuteNonQuery();
        }
        catch (Exception ex)
        {
          var preview = stmt.Length > 200 ? stmt.Substring(0, 200) + "..." : stmt;
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

    public static long ScalarLong(string sql, params (string, object?)[] p)
    {
      using var cmd = Conn!.CreateCommand();
      cmd.CommandText = sql;
      foreach (var (k, v) in p) cmd.Parameters.AddWithValue(k, v ?? DBNull.Value);
      return Convert.ToInt64(cmd.ExecuteScalar() ?? 0);
    }

    public static int Exec(string sql, params (string, object?)[] p)
    {
      using var cmd = Conn!.CreateCommand();
      cmd.CommandText = sql;
      foreach (var (k, v) in p) cmd.Parameters.AddWithValue(k, v ?? DBNull.Value);
      return cmd.ExecuteNonQuery();
    }

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
  }
}
