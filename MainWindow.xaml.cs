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
      RefreshCashbox(); // show zeros even before a session
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
      if (Database.Conn is null) { MessageBox.Show(this, "Open a session first."); return; }

      var view = new BuyInView();
      view.Cancelled += (_, __) => ShowTransactions();
      view.Confirmed += (_, args) =>
      {
        var member = args.MemberNumber;
        var cashAmt = args.TotalCents / 100.0;
        var chipsAmt = cashAmt;

        Database.InTransaction(tx =>
        {
          EnsurePlayer(member, tx);
          var pid = PlayerIdByMember(member, tx);
          var sid = LatestSessionId(tx);

          Database.Exec(
            "INSERT INTO transactions(session_id, player_id, type, cash_amt, chips_amt, method, staff) " +
            "VALUES ($s,$p,'BUYIN',$cash,$chips,'Cash','Dealer')",
            tx, ("$s", sid), ("$p", pid), ("$cash", cashAmt), ("$chips", chipsAmt)
          );
          var txId = Database.ScalarLong("SELECT last_insert_rowid()", tx);

          // money enters cashbox as positive movement
          foreach (var kv in args.DenomCounts.Where(kv => kv.Value > 0))
          {
            Database.Exec(
              "INSERT INTO cashbox_movements(session_id, denom_cents, delta_qty, reason, player_id, tx_id) " +
              "VALUES ($s, $d, $q, 'BUYIN', $p, $tx)",
              tx, ("$s", sid), ("$d", kv.Key), ("$q", kv.Value), ("$p", pid), ("$tx", txId)
            );
          }
        });

        RefreshTransactions();
        RefreshCashbox();
        StatusText.Text = $"Buy-in recorded for {member} — {cashAmt.ToString("C", CultureInfo.GetCultureInfo("en-AU"))}";
        ShowTransactions();
      };

      ShowScreen(view);
    }

    // ===== Activity: Cash-out inline =====
    private void CashOut_Click(object sender, RoutedEventArgs e)
    {
      if (Database.Conn is null) { MessageBox.Show(this, "Open a session first."); return; }

      var view = new CashOutView();
      view.Back += (_, __) => ShowTransactions();
      view.Confirmed += (_, args) =>
      {
        var member = args.MemberNumber;
        var cashAmt = args.TotalCents / 100.0;
        var chipsAmt = cashAmt;

        Database.InTransaction(tx =>
        {
          EnsurePlayer(member, tx);
          var pid = PlayerIdByMember(member, tx);
          var sid = LatestSessionId(tx);

          Database.Exec(
            "INSERT INTO transactions(session_id, player_id, type, cash_amt, chips_amt, method, staff) " +
            "VALUES ($s,$p,'CASHOUT',$cash,$chips,'Cash','Dealer')",
            tx, ("$s", sid), ("$p", pid), ("$cash", cashAmt), ("$chips", chipsAmt)
          );
          var txId = Database.ScalarLong("SELECT last_insert_rowid()", tx);

          // cash leaves cashbox as negative movement
          foreach (var kv in args.PayoutPlan.Where(kv => kv.Value > 0))
          {
            Database.Exec(
              "INSERT INTO cashbox_movements(session_id, denom_cents, delta_qty, reason, player_id, tx_id) " +
              "VALUES ($s, $d, $q, 'CASHOUT', $p, $tx)",
              tx, ("$s", sid), ("$d", kv.Key), ("$q", -kv.Value), ("$p", pid), ("$tx", txId)
            );
          }
        });

        RefreshTransactions();
        RefreshCashbox();
        StatusText.Text = $"Cash-out paid to {member} — {cashAmt.ToString("C", CultureInfo.GetCultureInfo("en-AU"))}";
        ShowTransactions();
      };

      ShowScreen(view);
    }

    // ===== Cashbox: Add Float (additive) =====
    private void AddFloat_Click(object? sender, RoutedEventArgs e)
    {
      if (Database.Conn is null) { MessageBox.Show(this, "Open a session first."); return; }

      var sid = Database.ScalarLong("SELECT session_id FROM sessions ORDER BY session_id DESC LIMIT 1");

      // zeros preset – we are adding money in
      var preset = new Dictionary<int,int>
      {
        { 5,0 },{10,0},{20,0},{50,0},
        {100,0},{200,0},{500,0},{1000,0},{2000,0},{5000,0},{10000,0}
      };

      // use your existing touch float editor dialog
      var dlg = new FloatWindow(preset) { Owner = this };
      if (dlg.ShowDialog() != true) return;

      Database.InTransaction(tx =>
      {
        foreach (var kv in dlg.Counts)
        {
          if (kv.Value <= 0) continue;
          Database.Exec(
            "INSERT INTO cashbox_movements(session_id, denom_cents, delta_qty, reason, player_id, tx_id) " +
            "VALUES ($s, $d, $q, 'FLOAT_ADD', NULL, NULL)",
            tx, ("$s", sid), ("$d", kv.Key), ("$q", kv.Value)
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
        "SELECT tx_id, time, type, player_id, session_id, cash_amt, chips_amt, method, staff " +
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
      // Always build rows (show zeros if no DB/session)
      var counts = AllDenoms.ToDictionary(d => d, d => 0);
      string? err = null;

      try
      {
        if (Database.Conn != null)
        {
          long sid = Database.ScalarLong("SELECT session_id FROM sessions ORDER BY session_id DESC LIMIT 1");
          if (sid > 0)
          {
            // baseline (optional)
            var floatDt = Database.Query("SELECT denom_cents, qty FROM cashbox_float WHERE session_id=$s", ("$s", sid));
            foreach (DataRow r in floatDt.Rows)
            {
              int d = Convert.ToInt32(r["denom_cents"]);
              int q = Convert.ToInt32(r["qty"]);
              if (counts.ContainsKey(d)) counts[d] += q;
            }

            // movements (buy-in +, cash-out -, float_add +)
            var movDt = Database.Query(
              "SELECT denom_cents, COALESCE(SUM(delta_qty),0) AS qty " +
              "FROM cashbox_movements WHERE session_id=$s GROUP BY denom_cents", ("$s", sid));
            foreach (DataRow r in movDt.Rows)
            {
              int d = Convert.ToInt32(r["denom_cents"]);
              int q = Convert.ToInt32(r["qty"]);
              if (counts.ContainsKey(d)) counts[d] += q;
            }
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
    private void EnsurePlayer(string member, SqliteTransaction? tx)
    {
      Database.Exec(
        "INSERT OR IGNORE INTO players(member_no, first_name, last_name, display_name) VALUES ($m,'New','Player',$m)",
        tx, ("$m", member)
      );
    }

    private long LatestSessionId(SqliteTransaction? tx)
      => Database.ScalarLong("SELECT session_id FROM sessions ORDER BY session_id DESC LIMIT 1", tx);

    private long PlayerIdByMember(string member, SqliteTransaction? tx)
      => Database.ScalarLong("SELECT player_id FROM players WHERE member_no=$m", tx, ("$m", member));
  }
}
