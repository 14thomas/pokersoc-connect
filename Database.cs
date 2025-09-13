using Microsoft.Data.Sqlite;
using System;
using System.Data;
using System.IO;
using System.Reflection;

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

    public static void EnsureSchemaFromResource(Assembly asm, string resourceName)
    {
      using var stream = asm.GetManifestResourceStream(resourceName)
        ?? throw new InvalidOperationException($"Embedded resource not found: {resourceName}");
      using var reader = new StreamReader(stream);
      var sql = reader.ReadToEnd();

      using var cmd = Conn!.CreateCommand();
      cmd.CommandText = sql;  // multiple statements OK
      cmd.ExecuteNonQuery();
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
