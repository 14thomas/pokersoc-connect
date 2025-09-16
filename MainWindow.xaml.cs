using Microsoft.Data.Sqlite;
using pokersoc_connect.Views;
using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace pokersoc_connect
{
  public partial class MainWindow : Window
  {
    // Only the denominations you actually have (chips + plaques + cash notes/coins used)
    private static readonly int[] AllDenoms = { 5, 25, 100, 200, 500, 2500, 10000 };

    public MainWindow()
    {
      InitializeComponent();
      ShowTransactions();
      RefreshActivity();
      RefreshCashbox();
      TxGrid.MouseDoubleClick += TxGrid_MouseDoubleClick;
    }

    // ===== Screen switching =====
    private void ShowTransactions()
    {
      ScreenHost.Visibility = Visibility.Collapsed;
      ScreenHost.Content = null;
      TxGrid.Visibility = Visibility.Visible;
    }

    private void ShowScreen(UserControl view)
    {
      TxGrid.Visibility = Visibility.Collapsed;
      ScreenHost.Content = view;
      ScreenHost.Visibility = Visibility.Visible;
    }

    // ===== Activity: Buy-in inline =====
    private void BuyIn_Click(object sender, RoutedEventArgs e)
    {
      if (Database.Conn is null) { MessageBox.Show(this, "Open a session (DB file) first."); return; }

      var view = new BuyInView();
      view.Cancelled += (_, __) => ShowTransactions();
      view.Confirmed += (_, args) =>
      {
        var playerId = args.MemberNumber;
        var cashAmt  = args.TotalCents / 100.0;

        Database.InTransaction(tx =>
        {
          EnsurePlayer(playerId, tx);

          Database.Exec(
            "INSERT INTO transactions(player_id, type, cash_amt, method, staff) " +
            "VALUES ($p, 'BUYIN', $cash, 'Cash', 'Dealer')",
            tx, ("$p", playerId), ("$cash", cashAmt)
          );

          var txId = Database.ScalarLong("SELECT last_insert_rowid()", tx);

          foreach (var kv in args.DenomCounts.Where(kv => kv.Value > 0))
          {
            Database.Exec(
              "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id) " +
              "VALUES ($d, $q, 'BUYIN', $p, $tx)",
              tx, ("$d", kv.Key), ("$q", kv.Value), ("$p", playerId), ("$tx", txId)
            );
          }
        });

        RefreshActivity();
        RefreshCashbox();
        StatusText.Text = $"Buy-in recorded for {playerId} — {cashAmt.ToString("C", CultureInfo.GetCultureInfo("en-AU"))}";
        ShowTransactions();
      };

      ShowScreen(view);
    }

    // ===== Activity: Cash-out inline =====
    private void CashOut_Click(object sender, RoutedEventArgs e)
    {
      if (Database.Conn is null) { MessageBox.Show(this, "Open a session (DB file) first."); return; }

      var view = new CashOutView();
      view.Back += (_, __) => ShowTransactions();
      view.Confirmed += (_, args) =>
      {
        var playerId = args.MemberNumber;
        var cashAmt  = args.TotalCents / 100.0;

        Database.InTransaction(tx =>
        {
          EnsurePlayer(playerId, tx);

          Database.Exec(
            "INSERT INTO transactions(player_id, type, cash_amt, method, staff) " +
            "VALUES ($p, 'CASHOUT', $cash, 'Cash', 'Dealer')",
            tx, ("$p", playerId), ("$cash", cashAmt)
          );

          var txId = Database.ScalarLong("SELECT last_insert_rowid()", tx);

          // 1) Record cash paid out (negative qty in cashbox)
          foreach (var kv in args.PayoutPlan.Where(kv => kv.Value > 0))
          {
            Database.Exec(
              "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id) " +
              "VALUES ($d, $q, 'CASHOUT', $p, $tx)",
              tx, ("$d", kv.Key), ("$q", -kv.Value), ("$p", playerId), ("$tx", txId)
            );
          }

          // 2) Record chips turned in (store per tx so we can show the colour breakdown)
          foreach (var kv in args.ChipsIn.Where(kv => kv.Value > 0))
          {
            Database.Exec(
              "INSERT INTO tx_chips(tx_id, denom_cents, qty) VALUES ($tx, $d, $q) " +
              "ON CONFLICT(tx_id, denom_cents) DO UPDATE SET qty = qty + EXCLUDED.qty",
              tx, ("$tx", txId), ("$d", kv.Key), ("$q", kv.Value)
            );
          }
        });

        RefreshActivity();
        RefreshCashbox();
        StatusText.Text = $"Cash-out paid to {playerId} — {cashAmt.ToString("C", CultureInfo.GetCultureInfo("en-AU"))}";
        ShowTransactions();
      };

      ShowScreen(view);
    }

    // ===== Cashbox: Add Float (additive) =====
    private void AddFloat_Click(object? sender, RoutedEventArgs e)
    {
      if (Database.Conn is null) { MessageBox.Show(this, "Open a session (DB file) first."); return; }

      var preset = AllDenoms.ToDictionary(d => d, d => 0);
      var dlg = new FloatWindow(preset) { Owner = this };
      if (dlg.ShowDialog() != true) return;

      Database.InTransaction(tx =>
      {
        var batchId = Guid.NewGuid().ToString("N");
        foreach (var kv in dlg.Counts)
        {
          if (kv.Value <= 0) continue;
          Database.Exec(
            "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id, batch_id) " +
            "VALUES ($d, $q, 'FLOAT_ADD', NULL, NULL, $batch)",
            tx, ("$d", kv.Key), ("$q", kv.Value), ("$batch", batchId)
          );
        }
      });

      RefreshActivity();
      RefreshCashbox();
      StatusText.Text = "Float added to cashbox.";
    }

    // ===== Tabs: Auto-refresh Cashbox when selected =====
    private void MainTabs_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      if (MainTabs.SelectedItem is TabItem tab && tab.Header?.ToString() == "Cashbox")
        RefreshCashbox();
    }

    // ===== Activity feed =====
    private void RefreshActivity()
    {
      var dt = Database.Query(@"
WITH float_batches AS (
  SELECT 
    MIN(time) AS time,
    'FLOAT_ADD' AS type,
    NULL AS player_id,
    SUM(denom_cents * delta_qty) / 100.0 AS cash_amt,
    NULL AS method,
    'Dealer' AS staff,
    COALESCE(batch_id, printf('legacy-%d', move_id)) AS batch_id
  FROM cashbox_movements
  WHERE reason = 'FLOAT_ADD'
  GROUP BY COALESCE(batch_id, printf('legacy-%d', move_id))
)
SELECT 
  tx_id            AS activity_key,
  time,
  type,
  player_id,
  cash_amt,
  method,
  staff,
  NULL             AS batch_id,
  'TX'             AS activity_kind
FROM transactions
UNION ALL
SELECT 
  NULL             AS activity_key,
  time,
  type,
  player_id,
  cash_amt,
  method,
  staff,
  batch_id,
  'FLOAT'          AS activity_kind
FROM float_batches
ORDER BY time DESC
");
      TxGrid.ItemsSource = dt.DefaultView;
    }

    // ===== Cashbox =====
    private sealed class CashboxRow
    {
      public string Denomination { get; set; } = "";
      public int    Count        { get; set; }
      public string Value        { get; set; } = "";
    }

    private void RefreshCashbox()
    {
      var counts = AllDenoms.ToDictionary(d => d, d => 0);
      string? err = null;

      try
      {
        if (Database.Conn != null)
        {
          var floatDt = Database.Query("SELECT denom_cents, qty FROM cashbox_float");
          foreach (DataRow r in floatDt.Rows)
          {
            int d = Convert.ToInt32(r["denom_cents"]);
            int q = Convert.ToInt32(r["qty"]);
            if (counts.ContainsKey(d)) counts[d] += q;
          }

          var movDt = Database.Query("SELECT denom_cents, COALESCE(SUM(delta_qty),0) AS qty FROM cashbox_movements GROUP BY denom_cents");
          foreach (DataRow r in movDt.Rows)
          {
            int d = Convert.ToInt32(r["denom_cents"]);
            int q = Convert.ToInt32(r["qty"]);
            if (counts.ContainsKey(d)) counts[d] += q;
          }
        }
      }
      catch (Exception ex)
      {
        err = ex.Message;
      }

      var au = CultureInfo.GetCultureInfo("en-AU");
      long totalCents = 0;
      var rows = new List<CashboxRow>();

      foreach (var d in AllDenoms)
      {
        int c = counts[d];
        long value = (long)c * d;
        totalCents += value;

        rows.Add(new CashboxRow
        {
          Denomination = DenomLabel(d),
          Count = c,
          Value = (value / 100.0).ToString("C", au)
        });
      }

      CashboxGrid.AutoGenerateColumns = true;
      CashboxGrid.ItemsSource = rows;
      CashboxTotalText.Text = (totalCents / 100.0).ToString("C", au);

      if (err != null)
        StatusText.Text = $"Cashbox (showing zeros): {err}";
    }

    // ===== helpers =====
    private void EnsurePlayer(string playerId, SqliteTransaction? tx)
    {
      Database.Exec(
        "INSERT OR IGNORE INTO players(player_id, first_name, last_name, display_name) " +
        "VALUES ($id,'','','')",
        tx, ("$id", playerId)
      );
      Database.Exec(
        "UPDATE players SET display_name = COALESCE(NULLIF(display_name,''), $id) WHERE player_id=$id",
        tx, ("$id", playerId)
      );
    }

    private void TxGrid_MouseDoubleClick(object sender, System.Windows.Input.MouseButtonEventArgs e)
    {
      if (TxGrid.SelectedItem is DataRowView row)
      {
        var kind = row["activity_kind"]?.ToString();
        if (kind == "TX")
        {
          long txId = row["activity_key"] is DBNull ? 0 : Convert.ToInt64(row["activity_key"]);
          var type = row["type"]?.ToString() ?? "";
          ShowTransactionBreakdown(txId, type);
        }
        else if (kind == "FLOAT")
        {
          var batchId = row["batch_id"]?.ToString();
          ShowFloatBreakdown(batchId);
        }
      }
    }

    private void ShowTransactionBreakdown(long txId, string type)
    {
      var au = CultureInfo.GetCultureInfo("en-AU");

      // Chips turned in (tx_chips)
      var chipsDt = Database.Query(@"
SELECT denom_cents, qty
FROM tx_chips
WHERE tx_id = $tx
ORDER BY denom_cents DESC", ("$tx", txId));
      var chips = chipsDt.Rows.Cast<DataRow>()
        .Select(r => (denom: Convert.ToInt32(r["denom_cents"]), qty: Convert.ToInt32(r["qty"])))
        .ToList();

      // Cash movement for this tx (positive for BUYIN, negative for CASHOUT)
      var cashDt = Database.Query(@"
SELECT denom_cents, SUM(delta_qty) AS qty
FROM cashbox_movements
WHERE tx_id = $tx
GROUP BY denom_cents
ORDER BY denom_cents DESC", ("$tx", txId));
      var cash = cashDt.Rows.Cast<DataRow>()
        .Select(r => (denom: Convert.ToInt32(r["denom_cents"]), qty: Convert.ToInt32(r["qty"])))
        .ToList();

      string title = type == "CASHOUT" ? "Cash-out breakdown" : "Buy-in breakdown";
      string subtitle = type == "CASHOUT"
        ? "Chips turned in + cash paid out"
        : "Cash received";

      // For BUYIN we may not have chips list; we only show cash received.
      var view = new BreakdownView(title, subtitle, type == "CASHOUT" ? chips : Enumerable.Empty<(int,int)>(), cash, isCashOut: type == "CASHOUT");
      view.Back += (_, __) => ShowTransactions();
      ShowScreen(view);
    }

    private void ShowFloatBreakdown(string? batchId)
    {
      if (string.IsNullOrWhiteSpace(batchId))
      {
        MessageBox.Show(this, "Missing float batch id.", "Details");
        return;
      }

      DataTable dt;

      if (batchId.StartsWith("legacy-") && long.TryParse(batchId.AsSpan("legacy-".Length), out var moveId))
      {
        dt = Database.Query(@"
SELECT denom_cents, delta_qty AS qty
FROM cashbox_movements
WHERE move_id = $id
ORDER BY denom_cents DESC", ("$id", moveId));
      }
      else
      {
        dt = Database.Query(@"
SELECT denom_cents, SUM(delta_qty) AS qty
FROM cashbox_movements
WHERE reason = 'FLOAT_ADD' AND batch_id = $b
GROUP BY denom_cents
ORDER BY denom_cents DESC", ("$b", batchId));
      }

      var lines = dt.Rows.Cast<DataRow>()
        .Select(r => (denom: Convert.ToInt32(r["denom_cents"]), qty: Convert.ToInt32(r["qty"])))
        .ToList();

      var view = new BreakdownView("Float addition", null,
                                   Enumerable.Empty<(int,int)>(),   // no chips for float add
                                   lines,
                                   isCashOut: false);
      view.Back += (_, __) => ShowTransactions();
      ShowScreen(view);
    }

    private static string DenomLabel(int cents) => cents switch
    {
      5 => "5c",
      25 => "25c",
      100 => "$1",
      200 => "$2",
      500 => "$5",
      2500 => "$25",
      10000 => "$100",
      _ => $"{cents}c"
    };
  }
}
