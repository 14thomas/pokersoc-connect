using Microsoft.Data.Sqlite;
using pokersoc_connect.Views;
using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.IO;

namespace pokersoc_connect
{
  public partial class MainWindow : Window
  {
    // Australian currency denominations only (for cashbox)
    private static readonly int[] CashDenoms = { 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    
    // All denominations including poker chips (for buy-ins, cash-outs, etc.)
    private static readonly int[] AllDenoms = { 5, 10, 20, 25, 50, 100, 200, 500, 1000, 2000, 2500, 5000, 10000 };

    public MainWindow()
    {
      InitializeComponent();
      ShowTransactions();
      RefreshActivity();
      TxGrid.MouseDoubleClick += TxGrid_MouseDoubleClick;
    }

    // ===== Fullscreen functionality =====
    private void Window_KeyDown(object sender, System.Windows.Input.KeyEventArgs e)
    {
      if (e.Key == System.Windows.Input.Key.F11)
      {
        ToggleFullscreen();
        e.Handled = true;
      }
    }

    private void ToggleFullscreen()
    {
      if (WindowStyle == WindowStyle.None)
      {
        // Exit fullscreen
        WindowStyle = WindowStyle.SingleBorderWindow;
        WindowState = WindowState.Normal;
        ResizeMode = ResizeMode.CanResize;
        Topmost = false;
        Left = (SystemParameters.WorkArea.Width - Width) / 2;
        Top = (SystemParameters.WorkArea.Height - Height) / 2;
      }
      else
      {
        // Enter fullscreen - hide taskbar
        WindowStyle = WindowStyle.None;
        WindowState = WindowState.Maximized;
        ResizeMode = ResizeMode.NoResize;
        Topmost = true;
        Left = 0;
        Top = 0;
        Width = SystemParameters.PrimaryScreenWidth;
        Height = SystemParameters.PrimaryScreenHeight;
      }
    }

    // ===== Screen switching =====
    private void ShowTransactions()
    {
      ScreenHost.Visibility = Visibility.Collapsed;
      SettingsHost.Visibility = Visibility.Collapsed;
      FoodHost.Visibility = Visibility.Collapsed;
      ScreenHost.Content = null;
      MainContent.Visibility = Visibility.Visible;
    }

    private void ShowScreen(UserControl view)
    {
      MainContent.Visibility = Visibility.Collapsed;
      ScreenHost.Content = view;
      ScreenHost.Visibility = Visibility.Visible;
    }

    private void ShowSettings()
    {
      // Hide main content and other overlays
      MainContent.Visibility = Visibility.Collapsed;
      SettingsHost.Visibility = Visibility.Collapsed;
      FoodHost.Visibility = Visibility.Collapsed;
      ScreenHost.Visibility = Visibility.Collapsed;
      
      // Show settings content in ScreenHost
      var settingsView = new Views.SettingsView();
        settingsView.SettingsChanged += (s, e) => 
        {
          RefreshActivity();
        };
      settingsView.CloseRequested += (s, e) => ShowTransactions();
      ScreenHost.Content = settingsView;
      ScreenHost.Visibility = Visibility.Visible;
    }

    private void ShowCashbox()
    {
      // Hide main content and other overlays
      MainContent.Visibility = Visibility.Collapsed;
      SettingsHost.Visibility = Visibility.Collapsed;
      FoodHost.Visibility = Visibility.Collapsed;
      ScreenHost.Visibility = Visibility.Collapsed;
      
      // Show cashbox content in ScreenHost
      var cashboxView = new Views.CashboxView();
      cashboxView.CloseRequested += (s, e) => {
        RefreshActivity();
        ShowTransactions();
      };
      ScreenHost.Content = cashboxView;
      ScreenHost.Visibility = Visibility.Visible;
    }

    private void ShowFood()
    {
      // Hide main content and other overlays
      MainContent.Visibility = Visibility.Collapsed;
      SettingsHost.Visibility = Visibility.Collapsed;
      FoodHost.Visibility = Visibility.Collapsed;
      ScreenHost.Visibility = Visibility.Collapsed;
      
      // Show food content in ScreenHost
      var foodView = new FoodCatalogView();
      foodView.BackToMain += (_, __) => ShowTransactions();
      foodView.GoToSettings += (_, __) => ShowSettings();
      foodView.ActivityRefreshRequested += (_, __) => RefreshActivity();
      ScreenHost.Content = foodView;
      ScreenHost.Visibility = Visibility.Visible;
    }

    private void ShowLostChips()
    {
      // Hide main content and other overlays
      MainContent.Visibility = Visibility.Collapsed;
      SettingsHost.Visibility = Visibility.Collapsed;
      FoodHost.Visibility = Visibility.Collapsed;
      ScreenHost.Visibility = Visibility.Collapsed;
      
      // Show lost chips content in ScreenHost
      var lostChipsView = new Views.LostChipsView();
      lostChipsView.CloseRequested += (s, e) => ShowTransactions();
      ScreenHost.Content = lostChipsView;
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
        var buyInAmt = args.BuyInAmountCents / 100.0;

        Database.InTransaction(tx =>
        {
          EnsurePlayer(playerId, tx);

          Database.Exec(
            "INSERT INTO transactions(player_id, type, cash_amt, method, staff) " +
            "VALUES ($p, 'BUYIN', $cash, 'Cash', 'Dealer')",
            tx, ("$p", playerId), ("$cash", buyInAmt)
          );

          var txId = Database.ScalarLong("SELECT last_insert_rowid()", tx);

          // Record cash received (positive qty in cashbox)
          foreach (var kv in args.CashReceived.Where(kv => kv.Value > 0))
          {
            Database.Exec(
              "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id) " +
              "VALUES ($d, $q, 'BUYIN', $p, $tx)",
              tx, ("$d", kv.Key), ("$q", kv.Value), ("$p", playerId), ("$tx", txId)
            );
          }

          // Record change given out (negative qty in cashbox)
          foreach (var kv in args.ChangeBreakdown.Where(kv => kv.Value > 0))
          {
            Database.Exec(
              "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id) " +
              "VALUES ($d, $q, 'CHANGE', $p, $tx)",
              tx, ("$d", kv.Key), ("$q", -kv.Value), ("$p", playerId), ("$tx", txId)
            );
          }
        });

        RefreshActivity();
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
        ShowTransactions();
      };

      ShowScreen(view);
    }

    // ===== Cashbox: Add Float (additive) =====
    private void AddFloat_Click(object? sender, RoutedEventArgs e)
    {
      if (Database.Conn is null) { MessageBox.Show(this, "Open a session (DB file) first."); return; }

      var preset = CashDenoms.ToDictionary(d => d, d => 0);
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
    }
    


    // ===== New Button Handlers =====
    private void Cashbox_Click(object sender, RoutedEventArgs e)
    {
        ShowCashbox();
    }

    private void Food_Click(object sender, RoutedEventArgs e)
    {
        ShowFood();
    }

    private void Settings_Click(object sender, RoutedEventArgs e)
    {
        ShowSettings();
    }

    private void LostChips_Click(object sender, RoutedEventArgs e)
    {
        ShowLostChips();
    }

    // ===== Activity feed =====
    private void RefreshActivity()
    {
      var dt = Database.Query(@"
SELECT 
  tx_id,
  time,
  type,
  player_id,
  cash_amt,
  NULL AS batch_id,
  'TX' AS activity_kind
FROM transactions
UNION ALL
SELECT 
  tx_id,
  time,
  activity_type AS type,
  player_id,
  amount_cents / 100.0 AS cash_amt,
  batch_id,
  activity_kind
FROM activity_log
ORDER BY time DESC
");
      TxGrid.ItemsSource = dt.DefaultView;
    }

    // ===== Cashbox =====
    // Cashbox functionality moved to CashboxView

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
          long txId = row["tx_id"] is DBNull ? 0 : Convert.ToInt64(row["tx_id"]);
          var type = row["type"]?.ToString() ?? "";
          ShowTransactionBreakdown(txId, type);
        }
        else if (kind == "FLOAT" || kind == "FLOAT_ADD")
        {
          var batchId = row["batch_id"]?.ToString();
          ShowFloatBreakdown(batchId);
        }
        else if (kind == "FOOD")
        {
          long saleId = row["tx_id"] is DBNull ? 0 : Convert.ToInt64(row["tx_id"]);
          ShowFoodSaleDetails(saleId);
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

      if (type == "BUYIN")
      {
        // For BUYIN: separate cash received from change given back
        var cashReceivedDt = Database.Query(@"
SELECT denom_cents, SUM(delta_qty) AS qty
FROM cashbox_movements
WHERE tx_id = $tx AND reason = 'BUYIN'
GROUP BY denom_cents
ORDER BY denom_cents DESC", ("$tx", txId));
        var cashReceived = cashReceivedDt.Rows.Cast<DataRow>()
          .Select(r => (denom: Convert.ToInt32(r["denom_cents"]), qty: Convert.ToInt32(r["qty"])))
          .ToList();

        var changeGivenDt = Database.Query(@"
SELECT denom_cents, ABS(SUM(delta_qty)) AS qty
FROM cashbox_movements
WHERE tx_id = $tx AND reason = 'CHANGE'
GROUP BY denom_cents
ORDER BY denom_cents DESC", ("$tx", txId));
        var changeGiven = changeGivenDt.Rows.Cast<DataRow>()
          .Select(r => (denom: Convert.ToInt32(r["denom_cents"]), qty: Convert.ToInt32(r["qty"])))
          .ToList();

        string title = "Buy-in breakdown";
        string subtitle = "Cash received + change given back";

        var view = new BreakdownView(title, subtitle, Enumerable.Empty<(int,int)>(), cashReceived, changeGiven, isCashOut: false);
        view.Back += (_, __) => ShowTransactions();
        ShowScreen(view);
      }
      else
      {
        // For CASHOUT: show chips turned in + cash paid out
        var cashDt = Database.Query(@"
SELECT denom_cents, SUM(delta_qty) AS qty
FROM cashbox_movements
WHERE tx_id = $tx
GROUP BY denom_cents
ORDER BY denom_cents DESC", ("$tx", txId));
        var cash = cashDt.Rows.Cast<DataRow>()
          .Select(r => (denom: Convert.ToInt32(r["denom_cents"]), qty: Convert.ToInt32(r["qty"])))
          .ToList();

        string title = "Cash-out breakdown";
        string subtitle = "Chips turned in + cash paid out";

        var view = new BreakdownView(title, subtitle, chips, cash, Enumerable.Empty<(int,int)>(), isCashOut: true);
        view.Back += (_, __) => ShowTransactions();
        ShowScreen(view);
      }
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

    private void ShowFoodSaleDetails(long saleId)
    {
      var view = new FoodSaleDetailsView(saleId);
      view.Back += (_, __) => ShowTransactions();
      ShowScreen(view);
    }



  }
}
