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
    private static readonly int[] AllDenoms = { 5,10,20,50,100,200,500,1000,2000,5000,10000 };

    public MainWindow()
    {
      InitializeComponent();
      ShowTransactions();
      RefreshTransactions();
      RefreshCashbox(); // render zeros even if DB empty
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
        var playerId = args.MemberNumber;               // scanned id IS the primary key
        var cashAmt  = args.TotalCents / 100.0;
        var chipsAmt = cashAmt;

        Database.InTransaction(tx =>
        {
          EnsurePlayer(playerId, tx);

          Database.Exec(
            "INSERT INTO transactions(player_id, type, cash_amt, chips_amt, method, staff) " +
            "VALUES ($p,'BUYIN',$cash,$chips,'Cash','Dealer')",
            tx, ("$p", playerId), ("$cash", cashAmt), ("$chips", chipsAmt)
          );
          var txId = Database.ScalarLong("SELECT last_insert_rowid()", tx);

          // money enters cashbox as positive movement
          foreach (var kv in args.DenomCounts.Where(kv => kv.Value > 0))
          {
            Database.Exec(
              "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id) " +
              "VALUES ($d, $q, 'BUYIN', $p, $tx)",
              tx, ("$d", kv.Key), ("$q", kv.Value), ("$p", playerId), ("$tx", txId)
            );
          }
        });

        RefreshTransactions();
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
        var playerId = args.MemberNumber;               // scanned id IS the primary key
        var cashAmt  = args.TotalCents / 100.0;
        var chipsAmt = cashAmt;

        Database.InTransaction(tx =>
        {
          EnsurePlayer(playerId, tx);

          Database.Exec(
            "INSERT INTO transactions(player_id, type, cash_amt, chips_amt, method, staff) " +
            "VALUES ($p,'CASHOUT',$cash,$chips,'Cash','Dealer')",
            tx, ("$p", playerId), ("$cash", cashAmt), ("$chips", chipsAmt)
          );
          var txId = Database.ScalarLong("SELECT last_insert_rowid()", tx);

          // cash leaves cashbox as negative movement
          foreach (var kv in args.PayoutPlan.Where(kv => kv.Value > 0))
          {
            Database.Exec(
              "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id) " +
              "VALUES ($d, $q, 'CASHOUT', $p, $tx)",
              tx, ("$d", kv.Key), ("$q", -kv.Value), ("$p", playerId), ("$tx", txId)
            );
          }
        });

        RefreshTransactions();
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

      var preset = new Dictionary<int,int>  // zeros (we ADD cash in)
      { {5,0},{10,0},{20,0},{50,0},{100,0},{200,0},{500,0},{1000,0},{2000,0},{5000,0},{10000,0} };

      var dlg = new FloatWindow(preset) { Owner = this };
      if (dlg.ShowDialog() != true) return;

      Database.InTransaction(tx =>
      {
        foreach (var kv in dlg.Counts)
        {
          if (kv.Value <= 0) continue;
          Database.Exec(
            "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id) " +
            "VALUES ($d, $q, 'FLOAT_ADD', NULL, NULL)",
            tx, ("$d", kv.Key), ("$q", kv.Value)
          );
        }
      });

      RefreshCashbox();
      StatusText.Text = "Float added to cashbox.";
    }

    // ===== Tabs: Auto-refresh Cashbox when selected =====
    private void MainTabs_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      if (MainTabs.SelectedItem is TabItem tab && tab.Header?.ToString() == "Cashbox")
        RefreshCashbox();
    }

    // ===== Data actions =====
    private void RefreshTransactions()
    {
      var dt = Database.Query(
        "SELECT tx_id, time, type, player_id, cash_amt, chips_amt, method, staff " +
        "FROM transactions ORDER BY tx_id DESC");
      TxGrid.ItemsSource = dt.DefaultView;
    }

    private sealed class CashboxRow
    {
      public string Denomination { get; set; } = "";
      public int    Count        { get; set; }
      public string Value        { get; set; } = "";
    }

    private void RefreshCashbox()
    {
      // Always build rows (show zeros if no DB)
      var counts = AllDenoms.ToDictionary(d => d, d => 0);
      string? err = null;

      try
      {
        if (Database.Conn != null)
        {
          // baseline
          var floatDt = Database.Query("SELECT denom_cents, qty FROM cashbox_float");
          foreach (DataRow r in floatDt.Rows)
          {
            int d = Convert.ToInt32(r["denom_cents"]);
            int q = Convert.ToInt32(r["qty"]);
            if (counts.ContainsKey(d)) counts[d] += q;
          }

          // movements
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
        err = ex.Message; // still render zeros
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
          Denomination = d switch
          {
            5 => "5c", 10 => "10c", 20 => "20c", 50 => "50c",
            100 => "$1", 200 => "$2", 500 => "$5",
            1000 => "$10", 2000 => "$20", 5000 => "$50", 10000 => "$100",
            _ => $"{d}c"
          },
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
      // Upsert a minimal player row so FK succeeds
      Database.Exec(
        "INSERT OR IGNORE INTO players(player_id, first_name, last_name, display_name) " +
        "VALUES ($id,'','','')",
        tx, ("$id", playerId)
      );
      // Optional: keep display_name in sync if empty
      Database.Exec(
        "UPDATE players SET display_name = COALESCE(NULLIF(display_name,''), $id) WHERE player_id=$id",
        tx, ("$id", playerId)
      );
    }
  }
}
