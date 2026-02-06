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

    // Current player ID (persists across tabs)
    private string _currentPlayerId = string.Empty;
    private bool _playerVerified = false;
    private bool _playerIsUnderage = false;
    private string _pendingPlayerId = string.Empty;

    private bool _allowClose = false;

    public MainWindow()
    {
      InitializeComponent();
      ShowTransactions();
      RefreshActivity();
      TxGrid.MouseDoubleClick += TxGrid_MouseDoubleClick;
      UpdatePlayerButtons(); // Ensure buttons are disabled initially
      this.Closing += MainWindow_Closing;
      this.Activated += MainWindow_Activated;
      this.Loaded += MainWindow_Loaded;
    }

    private void MainWindow_Loaded(object sender, RoutedEventArgs e)
    {
      // Focus the player ID box on startup
      FocusPlayerIdBoxIfEmpty();
    }

    private void MainWindow_Activated(object sender, EventArgs e)
    {
      // Re-focus player ID box when window is activated (clicked on, alt-tabbed to)
      FocusPlayerIdBoxIfEmpty();
    }

    private void FocusPlayerIdBoxIfEmpty()
    {
      // Only focus if the main content is visible and player is not verified
      if (MainContent.Visibility == Visibility.Visible && !_playerVerified)
      {
        CurrentPlayerIdBox.Focus();
        System.Windows.Input.Keyboard.Focus(CurrentPlayerIdBox);
      }
    }

    private void MainWindow_Closing(object? sender, System.ComponentModel.CancelEventArgs e)
    {
      if (!_allowClose)
      {
        e.Cancel = true;
        CloseConfirmationPanel.Visibility = Visibility.Visible;
      }
    }

    private void CancelCloseSession_Click(object sender, RoutedEventArgs e)
    {
      CloseConfirmationPanel.Visibility = Visibility.Collapsed;
    }

    private void ConfirmCloseSession_Click(object sender, RoutedEventArgs e)
    {
      _allowClose = true;
      this.Close();
    }

    // ===== Current Player Management =====
    private void CurrentPlayerIdBox_KeyDown(object sender, System.Windows.Input.KeyEventArgs e)
    {
      if (e.Key == System.Windows.Input.Key.Enter)
      {
        var playerId = CurrentPlayerIdBox.Text.Trim();
        
        if (!string.IsNullOrWhiteSpace(playerId))
        {
          // Reset any pending state first (handles double-scanning)
          NewPlayerPanel.Visibility = Visibility.Collapsed;
          UnderagePlayerPanel.Visibility = Visibility.Collapsed;
          _pendingPlayerId = string.Empty;
          
          // Check if this is a new player
          try
          {
            var isNew = Database.IsNewPlayer(playerId);
            
            if (isNew)
            {
              // Show inline verification panel
              _pendingPlayerId = playerId;
              NewPlayerIdText.Text = $"Player ID: {playerId}\n\nPlease verify this player is over 18 years old before continuing.\nThis player will be added as 'New Player'.";
              
              // Show the 18+ cutoff date (same day/month, 18 years ago)
              var cutoffDate = DateTime.Today.AddYears(-18);
              Age18CutoffText.Text = $"🎂 18+ if born on or before: {cutoffDate:dd MMM yyyy}";
              
              NewPlayerPanel.Visibility = Visibility.Visible;
              _playerVerified = false;
              _playerIsUnderage = false;
              
              // Lock the field to prevent accidental edits during verification
              CurrentPlayerIdBox.IsReadOnly = true;
              CurrentPlayerIdBox.Background = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(255, 255, 200));
            }
            else
            {
              // Existing player - set as current
              _currentPlayerId = playerId;
              _playerVerified = true;
              
              // Check if player is underage
              _playerIsUnderage = Database.IsPlayerUnderage(playerId);
              
              // Lock the player ID field with appropriate color
              CurrentPlayerIdBox.IsReadOnly = true;
              if (_playerIsUnderage)
              {
                CurrentPlayerIdBox.Background = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(255, 200, 200));
                UnderagePlayerPanel.Visibility = Visibility.Visible;
              }
              else
              {
                CurrentPlayerIdBox.Background = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(220, 255, 220));
              }
              
              // Show player info if they have non-default values
              ShowPlayerInfo(playerId);
              
              // Log attendance (even for underage players)
              Database.LogPlayerAttendance(playerId);
            }
          }
          catch (Exception ex)
          {
            MessageBox.Show($"Error checking player: {ex.Message}", "Error", 
              MessageBoxButton.OK, MessageBoxImage.Error);
          }
        }
        
        UpdatePlayerButtons();
        e.Handled = true;
      }
    }

    private void CurrentPlayerIdBox_TextChanged(object sender, TextChangedEventArgs e)
    {
      // When text changes (typing), clear verification state
      if (!_playerVerified)
      {
        _currentPlayerId = string.Empty;
      }
      UpdatePlayerButtons();
    }

    private void ClearPlayer_Click(object sender, RoutedEventArgs e)
    {
      _currentPlayerId = string.Empty;
      _pendingPlayerId = string.Empty;
      _playerVerified = false;
      _playerIsUnderage = false;
      CurrentPlayerIdBox.IsReadOnly = false;
      CurrentPlayerIdBox.Background = System.Windows.Media.Brushes.White;
      CurrentPlayerIdBox.Text = string.Empty;
      NewPlayerPanel.Visibility = Visibility.Collapsed;
      UnderagePlayerPanel.Visibility = Visibility.Collapsed;
      PlayerInfoPanel.Visibility = Visibility.Collapsed;
      CurrentPlayerIdBox.Focus();
      UpdatePlayerButtons();
    }
    
    private void ShowPlayerInfo(string playerId)
    {
      PlayerInfoStack.Children.Clear();
      PlayerInfoPanel.Visibility = Visibility.Collapsed;
      
      var details = Database.GetPlayerDetails(playerId);
      if (details == null) return;
      
      // Define which fields to show and their display labels
      var fieldsToShow = new List<(string key, string label)>
      {
        ("display_name", "Name"),
        ("degree", "Degree"),
        ("study_year", "Year"),
        ("student_number", "Student #"),
        ("email", "Email"),
        ("arc_member", "ARC Member")
      };
      
      bool hasAnyInfo = false;
      
      foreach (var (key, label) in fieldsToShow)
      {
        if (details.TryGetValue(key, out var value))
        {
          // Skip default values
          if (string.IsNullOrWhiteSpace(value) || 
              value == "None" || 
              value == "New Player" ||
              value == "N/A")
            continue;
          
          hasAnyInfo = true;
          
          var tb = new System.Windows.Controls.TextBlock
          {
            FontSize = 12,
            Margin = new Thickness(0, 1, 0, 1)
          };
          
          // Bold label, normal value
          tb.Inlines.Add(new System.Windows.Documents.Run($"{label}: ") { FontWeight = FontWeights.Bold });
          tb.Inlines.Add(new System.Windows.Documents.Run(value));
          
          PlayerInfoStack.Children.Add(tb);
        }
      }
      
      if (hasAnyInfo)
      {
        PlayerInfoPanel.Visibility = Visibility.Visible;
      }
    }

    private void VerifyNewPlayer_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        // Add the new player to the database with default values
        Database.Exec(
          "INSERT INTO players(player_id, display_name) " +
          "VALUES ($id, 'New Player')",
          ("$id", _pendingPlayerId)
        );
        
        // Set as current player and enable buttons
        _currentPlayerId = _pendingPlayerId;
        _playerVerified = true;
        CurrentPlayerIdBox.Text = _currentPlayerId;
        NewPlayerPanel.Visibility = Visibility.Collapsed;
        
        // Lock the player ID field
        CurrentPlayerIdBox.IsReadOnly = true;
        CurrentPlayerIdBox.Background = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(220, 255, 220));
        
        UpdatePlayerButtons();
        
        // Log attendance
        Database.LogPlayerAttendance(_currentPlayerId);
        
        // Show success message briefly (optional - you can remove this if you want)
        // MessageBox.Show($"Player {_currentPlayerId} added successfully!", "Success", 
        //   MessageBoxButton.OK, MessageBoxImage.Information);
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error adding player: {ex.Message}", "Error", 
          MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void VerifyNewPlayerUnderage_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        // Add the new player to the database as underage
        Database.Exec(
          "INSERT INTO players(player_id, display_name, is_underage) " +
          "VALUES ($id, 'New Player', 1)",
          ("$id", _pendingPlayerId)
        );
        
        // Set as current player but mark as underage
        _currentPlayerId = _pendingPlayerId;
        _playerVerified = true;
        _playerIsUnderage = true;
        CurrentPlayerIdBox.Text = _currentPlayerId;
        NewPlayerPanel.Visibility = Visibility.Collapsed;
        
        // Lock the player ID field with underage color
        CurrentPlayerIdBox.IsReadOnly = true;
        CurrentPlayerIdBox.Background = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(255, 200, 200));
        UnderagePlayerPanel.Visibility = Visibility.Visible;
        
        UpdatePlayerButtons();
        
        // Log attendance (even for underage players)
        Database.LogPlayerAttendance(_currentPlayerId);
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error adding player: {ex.Message}", "Error", 
          MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void CancelNewPlayer_Click(object sender, RoutedEventArgs e)
    {
      // Cancel - clear everything
      _currentPlayerId = string.Empty;
      _pendingPlayerId = string.Empty;
      _playerVerified = false;
      _playerIsUnderage = false;
      CurrentPlayerIdBox.IsReadOnly = false;
      CurrentPlayerIdBox.Background = System.Windows.Media.Brushes.White;
      CurrentPlayerIdBox.Text = string.Empty;
      NewPlayerPanel.Visibility = Visibility.Collapsed;
      UnderagePlayerPanel.Visibility = Visibility.Collapsed;
      UpdatePlayerButtons();
      CurrentPlayerIdBox.Focus();
    }

    private void UpdatePlayerButtons()
    {
      // Only enable buttons if player is verified AND not underage
      bool hasVerifiedPlayer = !string.IsNullOrWhiteSpace(_currentPlayerId) && _playerVerified && !_playerIsUnderage;
      BuyInButton.IsEnabled = hasVerifiedPlayer;
      CashOutButton.IsEnabled = hasVerifiedPlayer;
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
      
      // Refocus player ID box if empty
      FocusPlayerIdBoxIfEmpty();
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
      
      // Pre-fill player ID if one is set
      if (!string.IsNullOrEmpty(_currentPlayerId))
      {
        foodView.SetPlayerID(_currentPlayerId);
      }
      
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
      lostChipsView.CloseRequested += (s, e) =>
      {
        RefreshActivity();
        ShowTransactions();
      };
      ScreenHost.Content = lostChipsView;
      ScreenHost.Visibility = Visibility.Visible;
    }

    private void ShowPlayers()
    {
      // Hide main content and other overlays
      MainContent.Visibility = Visibility.Collapsed;
      SettingsHost.Visibility = Visibility.Collapsed;
      FoodHost.Visibility = Visibility.Collapsed;
      ScreenHost.Visibility = Visibility.Collapsed;
      
      // Show player management content in ScreenHost
      var playerView = new Views.PlayerManagementView();
      playerView.CloseRequested += (s, e) => ShowTransactions();
      ScreenHost.Content = playerView;
      ScreenHost.Visibility = Visibility.Visible;
    }

    // ===== Activity: Buy-in inline =====
    private void BuyIn_Click(object sender, RoutedEventArgs e)
    {
      if (Database.Conn is null) { MessageBox.Show(this, "Open a session (DB file) first."); return; }

      var view = new BuyInView();
      
      // Pre-fill player ID if one is set
      if (!string.IsNullOrEmpty(_currentPlayerId))
      {
        view.SetPlayerID(_currentPlayerId);
      }
      
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
      
      // Pre-fill player ID if one is set
      if (!string.IsNullOrEmpty(_currentPlayerId))
      {
        view.SetPlayerID(_currentPlayerId);
      }
      
      view.Back += (_, __) => ShowTransactions();
      view.Confirmed += (_, args) =>
      {
        var playerId = args.MemberNumber;
        var cashAmt  = args.TotalCents / 100.0;

        // Build transaction notes including food sales, tips, and extra cash
        var notes = new List<string>();
        if (args.FoodTotal > 0) notes.Add($"Food: {args.FoodTotal:C}");
        if (args.TipAmount > 0) notes.Add($"Tip: {args.TipAmount:C}");
        if (args.ExtraCashAmount > 0) notes.Add($"Extra Cash: {args.ExtraCashAmount:C}");
        var transactionNotes = notes.Count > 0 ? string.Join(", ", notes) : null;

        Database.InTransaction(tx =>
        {
          EnsurePlayer(playerId, tx);

          Database.Exec(
            "INSERT INTO transactions(player_id, type, cash_amt, method, staff, notes) " +
            "VALUES ($p, 'CASHOUT', $cash, 'Cash', 'Dealer', $notes)",
            tx, ("$p", playerId), ("$cash", cashAmt), ("$notes", transactionNotes)
          );

          var txId = Database.ScalarLong("SELECT last_insert_rowid()", tx);

          // Calculate change given (total chips - food - tip + extra cash)
          var changeGiven = cashAmt - args.FoodTotal - args.TipAmount + args.ExtraCashAmount;
          
          // Create ONE SINGLE activity log entry with complete cashout breakdown
          var activityNotes = $"Chips: {cashAmt:C} | Change: {changeGiven:C}";
          
          // Add detailed food items if any
          if (args.FoodTotal > 0 && args.FoodItems.Any())
          {
            System.Diagnostics.Debug.WriteLine($"CashOut: Processing {args.FoodItems.Count} food items:");
            foreach (var item in args.FoodItems)
            {
              System.Diagnostics.Debug.WriteLine($"  - {item.Key}: {item.Value}x");
            }
            
            // Extract just the item name (before the underscore in "ItemName_Price" format)
            var foodDetails = string.Join(", ", args.FoodItems.Select(item =>
            {
              var itemName = item.Key.Contains('_') ? item.Key.Substring(0, item.Key.LastIndexOf('_')) : item.Key;
              return item.Value > 1 ? $"{itemName} x{item.Value}" : itemName;
            }));
            activityNotes += $" | Food ({args.FoodTotal:C}): {foodDetails}";
            System.Diagnostics.Debug.WriteLine($"CashOut: Activity notes with food: '{activityNotes}'");
          }
          else if (args.FoodTotal > 0)
          {
            System.Diagnostics.Debug.WriteLine($"CashOut: Food total is {args.FoodTotal:C} but no items in dictionary");
            activityNotes += $" | Food: {args.FoodTotal:C}";
          }
          
          if (args.TipAmount > 0) activityNotes += $" | Tip: {args.TipAmount:C}";
          if (args.ExtraCashAmount > 0) activityNotes += $" | Cash Added: {args.ExtraCashAmount:C}";
          
          // Use different activity type if food was purchased during cashout  
          var activityType = args.FoodTotal > 0 ? "CASHOUT_SALE" : "CASHOUT";
            
          Database.Exec("INSERT INTO activity_log (activity_key, activity_type, activity_kind, player_id, tx_id, amount_cents, notes) VALUES (@key, @type, @kind, @player, @tx_id, @amount, @notes)", tx,
            ("@key", $"cashout_{txId}"),
            ("@type", activityType),
            ("@kind", "TX"),
            ("@player", playerId),
            ("@tx_id", txId),
            ("@amount", (int)Math.Round(cashAmt * 100)),
            ("@notes", activityNotes));
            
          // Add extra cash to cashbox if any
          if (args.ExtraCashAmount > 0)
          {
            // Break down extra cash into denominations and add to cashbox
            var extraCashCents = (int)(args.ExtraCashAmount * 100);
            var cashDenoms = new int[] { 10000, 5000, 2000, 1000, 500, 200, 100, 50, 20, 10, 5 };
            
            foreach (var denom in cashDenoms)
            {
              if (extraCashCents >= denom)
              {
                var count = extraCashCents / denom;
                extraCashCents -= count * denom;
                if (count > 0)
                {
                  Database.Exec(
                    "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id) " +
                    "VALUES ($d, $q, 'EXTRA_CASH', $p, $tx)",
                    tx, ("$d", denom), ("$q", count), ("$p", playerId), ("$tx", txId)
                  );
                }
              }
            }
          }
          
          // Add tips to tips table if any
          if (args.TipAmount > 0)
          {
            // Break down tip into denominations and add to tips
            var tipCents = (int)(args.TipAmount * 100);
            var cashDenoms = new int[] { 10000, 5000, 2000, 1000, 500, 200, 100, 50, 20, 10, 5 };
            
            foreach (var denom in cashDenoms)
            {
              if (tipCents >= denom)
              {
                var count = tipCents / denom;
                tipCents -= count * denom;
                if (count > 0)
                {
                  Database.Exec(
                    "INSERT INTO tips(denom_cents, qty, player_id, tx_id) " +
                    "VALUES ($d, $q, $p, $tx)",
                    tx, ("$d", denom), ("$q", count), ("$p", playerId), ("$tx", txId)
                  );
                }
              }
            }
          }
          

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
          
          // 3) Record food sales if any were purchased during cashout
          if (args.FoodTotal > 0 && args.FoodItems.Any())
          {
            System.Diagnostics.Debug.WriteLine($"Recording food sales during cashout: {args.FoodItems.Count} items");
            
            try
            {
              foreach (var foodItem in args.FoodItems)
              {
                // Parse item key: "ItemName_Price"
                var parts = foodItem.Key.Split(new[] { '_' }, 2);
                var itemName = parts[0];
                var itemPrice = parts.Length > 1 ? double.Parse(parts[1], CultureInfo.InvariantCulture) : 0;
                var quantity = foodItem.Value;
                var totalPrice = itemPrice * quantity;
              
              System.Diagnostics.Debug.WriteLine($"  Food item: {itemName} x{quantity} @ ${itemPrice} = ${totalPrice}");
              
              // Create a food sale record (using sales table, not transactions)
              Database.Exec(
                "INSERT INTO sales(player_id, staff, notes) VALUES ($p, $staff, $notes)",
                tx,
                ("$p", playerId),
                ("$staff", Environment.UserName),
                ("$notes", $"Food sale (via cashout): {itemName} x{quantity}")
              );
              
              var saleId = Database.ScalarLong("SELECT last_insert_rowid()", tx);
              
              // Try to find the product ID by name (query without transaction parameter)
              long? productId = null;
              try
              {
                var productIdResult = Database.ScalarLong(
                  "SELECT product_id FROM products WHERE name = $name LIMIT 1",
                  tx, ("$name", itemName)
                );
                if (productIdResult > 0)
                {
                  productId = productIdResult;
                }
              }
              catch
              {
                // Product not found, that's OK - we'll record the sale without product_id
                System.Diagnostics.Debug.WriteLine($"Product '{itemName}' not found in database");
              }
              
              // Add sale item
              Database.Exec(
                "INSERT INTO sale_items(sale_id, product_id, qty, unit_price) VALUES ($sale, $product, $qty, $price)",
                tx,
                ("$sale", saleId),
                ("$product", (object?)productId ?? DBNull.Value),
                ("$qty", quantity),
                ("$price", itemPrice)
              );
              
              // Add payment record (all cash, paid during cashout)
              // Record as one payment of the full amount
              Database.Exec(
                "INSERT INTO sale_payments(sale_id, method, denom_cents, qty) VALUES ($sale, 'CASH', $denom, 1)",
                tx,
                ("$sale", saleId),
                ("$denom", (int)(totalPrice * 100))
              );
              
              // Add cash to cashbox (food sales add to the cashbox)
              Database.Exec(
                "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id, notes) " +
                "VALUES ($denom, $delta, 'SALE', $player, $tx, $notes)",
                tx,
                ("$denom", (int)(totalPrice * 100)),
                ("$delta", 1), // One "unit" of this amount was added
                ("$player", playerId),
                ("$tx", saleId),
                ("$notes", $"Food sale (via cashout): {itemName} x{quantity}")
              );
              
              // Don't create separate FOOD activity log entry - it's already in the cashout details
              }
            }
            catch (Exception ex)
            {
              System.Diagnostics.Debug.WriteLine($"Error recording food sales during cashout: {ex.Message}");
              System.Windows.MessageBox.Show(
                $"Error recording food sales: {ex.Message}\n\nCashout will continue without food sale records.",
                "Food Sale Error",
                System.Windows.MessageBoxButton.OK,
                System.Windows.MessageBoxImage.Warning
              );
            }
          }
        });

        RefreshActivity();
        ShowTransactions();
        
        // Refresh cashbox if it's currently visible
        if (ScreenHost.Content is Views.CashboxView cashboxView)
        {
          cashboxView.RefreshCashbox();
        }
        
        // Refresh food view if it's currently visible
        if (ScreenHost.Content is FoodCatalogView foodView)
        {
          foodView.UpdateTotalMoneyDisplay();
        }
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

    private void Players_Click(object sender, RoutedEventArgs e)
    {
        ShowPlayers();
    }

    // ===== Activity feed =====
    private void RefreshActivity()
    {
      var dt = Database.Query(@"
SELECT 
  tx_id,
  strftime('%H:%M:%S', datetime(time, 'localtime')) AS time,
  type,
  player_id,
  cash_amt,
  NULL AS batch_id,
  'TX' AS activity_kind,
  NULL AS notes,
  time AS full_time
FROM transactions
WHERE tx_id NOT IN (SELECT tx_id FROM activity_log WHERE tx_id IS NOT NULL)
UNION ALL
SELECT 
  tx_id,
  strftime('%H:%M:%S', datetime(time, 'localtime')) AS time,
  activity_type AS type,
  player_id,
  amount_cents / 100.0 AS cash_amt,
  batch_id,
  activity_kind,
  notes,
  time AS full_time
FROM activity_log
ORDER BY full_time DESC
");
      TxGrid.ItemsSource = dt.DefaultView;
    }

    // ===== Cashbox =====
    // Cashbox functionality moved to CashboxView

    // ===== helpers =====
    private void EnsurePlayer(string playerId, SqliteTransaction? tx)
    {
      // Player should already exist (added during scan verification)
      // But use INSERT OR IGNORE as a safety net
      Database.Exec(
        "INSERT OR IGNORE INTO players(player_id, display_name) " +
        "VALUES ($id, 'New Player')",
        tx, ("$id", playerId)
      );
      Database.Exec(
        "UPDATE players SET display_name = COALESCE(NULLIF(display_name,''), 'New Player') WHERE player_id=$id",
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
        else if (kind == "LOST_CHIP")
        {
          var batchId = row["batch_id"]?.ToString();
          ShowLostChipsBreakdown(batchId);
        }
      }
    }

    private void ShowTransactionBreakdown(long txId, string type)
    {
      var au = CultureInfo.GetCultureInfo("en-AU");

      // Get transaction date/time and notes from activity_log (has detailed notes)
      var txInfo = Database.Query(@"
SELECT 
  datetime(time, 'localtime') AS formatted_time,
  notes
FROM activity_log 
WHERE tx_id = $tx
LIMIT 1", ("$tx", txId));
      
      string? dateTime = null;
      string? additionalInfo = null;
      
      if (txInfo.Rows.Count > 0)
      {
        dateTime = txInfo.Rows[0]["formatted_time"]?.ToString();
        var notesObj = txInfo.Rows[0]["notes"];
        if (notesObj != null && notesObj != DBNull.Value)
        {
          additionalInfo = notesObj.ToString();
          if (string.IsNullOrEmpty(additionalInfo))
          {
            additionalInfo = null;
          }
        }
      }

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

        var view = new BreakdownView(title, subtitle, Enumerable.Empty<(int,int)>(), cashReceived, changeGiven, false, dateTime, additionalInfo);
        view.EnableDeletion(txId, "BUYIN");
        view.Back += (_, __) => ShowTransactions();
        view.Deleted += (_, __) => { RefreshActivity(); ShowTransactions(); };
        ShowScreen(view);
      }
      else
      {
        // For CASHOUT: restructure to show GIVE vs TAKE
        
        try
        {
          // Cash paid out to player (negative qty in cashbox_movements with reason CASHOUT)
          var cashPaidOutDt = Database.Query(@"
SELECT denom_cents, ABS(SUM(delta_qty)) AS qty
FROM cashbox_movements
WHERE tx_id = $tx AND reason = 'CASHOUT'
GROUP BY denom_cents
ORDER BY denom_cents DESC", ("$tx", txId));
          var cashPaidOut = cashPaidOutDt.Rows.Cast<DataRow>()
            .Select(r => (denom: Convert.ToInt32(r["denom_cents"]), qty: Convert.ToInt32(r["qty"])))
            .ToList();

          // Extra cash added by player (positive qty in cashbox_movements with reason EXTRA_CASH)
          var extraCashDt = Database.Query(@"
SELECT denom_cents, SUM(delta_qty) AS qty
FROM cashbox_movements
WHERE tx_id = $tx AND reason = 'EXTRA_CASH'
GROUP BY denom_cents
ORDER BY denom_cents DESC", ("$tx", txId));
          var extraCash = extraCashDt.Rows.Cast<DataRow>()
            .Select(r => (denom: Convert.ToInt32(r["denom_cents"]), qty: Convert.ToInt32(r["qty"])))
            .ToList();

          // Get tips total - use COALESCE to handle NULL
          double tipAmount = 0;
          try
          {
            var tipsTotal = Database.Query(@"
SELECT COALESCE(SUM(denom_cents * qty), 0) AS total
FROM tips
WHERE tx_id = $tx", ("$tx", txId));
            if (tipsTotal.Rows.Count > 0)
            {
              var totalObj = tipsTotal.Rows[0]["total"];
              if (totalObj != null && totalObj != DBNull.Value)
              {
                tipAmount = Convert.ToInt64(totalObj) / 100.0;
              }
            }
          }
          catch (Exception ex)
          {
            System.Diagnostics.Debug.WriteLine($"Error getting tips: {ex.Message}");
            tipAmount = 0;
          }

          // Parse food total from additionalInfo
          double foodTotal = 0;
          try
          {
            if (!string.IsNullOrEmpty(additionalInfo))
            {
              System.Diagnostics.Debug.WriteLine($"Parsing food from: {additionalInfo}");
              // Try two formats:
              // 1. "Food ($5.00): items" (with items)
              // 2. "Food: $5.00" (without items)
              var foodMatch = System.Text.RegularExpressions.Regex.Match(additionalInfo, @"Food[:\s]+\(?[\$]?([\d,]+\.?\d*)\)?");
              if (foodMatch.Success && double.TryParse(foodMatch.Groups[1].Value.Replace(",", ""), out var parsed))
              {
                foodTotal = parsed;
                System.Diagnostics.Debug.WriteLine($"Food total parsed: {foodTotal}");
              }
              else
              {
                System.Diagnostics.Debug.WriteLine("Food regex didn't match");
              }
            }
            else
            {
              System.Diagnostics.Debug.WriteLine("additionalInfo is empty when parsing food");
            }
          }
          catch (Exception ex)
          {
            System.Diagnostics.Debug.WriteLine($"Error parsing food: {ex.Message}");
            foodTotal = 0;
          }

          System.Diagnostics.Debug.WriteLine($"Creating CashOutBreakdownView with foodTotal: {foodTotal}, tipAmount: {tipAmount}");

          string title = "Cash-out breakdown";
          string subtitle = "Transaction balance (GIVE = TAKE)";

          var view = new CashOutBreakdownView(title, subtitle, chips, cashPaidOut, extraCash, tipAmount, foodTotal, dateTime, additionalInfo);
          view.EnableDeletion(txId, type);
          view.Back += (_, __) => ShowTransactions();
          view.Deleted += (_, __) => { RefreshActivity(); ShowTransactions(); };
          ShowScreen(view);
        }
        catch (Exception ex)
        {
          MessageBox.Show(this, $"Error loading cashout breakdown: {ex.Message}\n\nStack trace: {ex.StackTrace}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
          ShowTransactions();
        }
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
      string? dateTime = null;

      if (batchId.StartsWith("legacy-") && long.TryParse(batchId.AsSpan("legacy-".Length), out var moveId))
      {
        dt = Database.Query(@"
SELECT denom_cents, delta_qty AS qty
FROM cashbox_movements
WHERE move_id = $id
ORDER BY denom_cents DESC", ("$id", moveId));
        
        // Get date/time for legacy float
        var timeInfo = Database.Query(@"
SELECT datetime(time, 'localtime') AS formatted_time
FROM cashbox_movements
WHERE move_id = $id
LIMIT 1", ("$id", moveId));
        if (timeInfo.Rows.Count > 0)
        {
          dateTime = timeInfo.Rows[0]["formatted_time"]?.ToString();
        }
      }
      else
      {
        dt = Database.Query(@"
SELECT denom_cents, SUM(delta_qty) AS qty
FROM cashbox_movements
WHERE reason = 'FLOAT_ADD' AND batch_id = $b
GROUP BY denom_cents
ORDER BY denom_cents DESC", ("$b", batchId));
        
        // Get date/time for batch float
        var timeInfo = Database.Query(@"
SELECT datetime(time, 'localtime') AS formatted_time
FROM cashbox_movements
WHERE batch_id = $b
LIMIT 1", ("$b", batchId));
        if (timeInfo.Rows.Count > 0)
        {
          dateTime = timeInfo.Rows[0]["formatted_time"]?.ToString();
        }
      }

      var lines = dt.Rows.Cast<DataRow>()
        .Select(r => (denom: Convert.ToInt32(r["denom_cents"]), qty: Convert.ToInt32(r["qty"])))
        .ToList();

      var view = new BreakdownView("Float addition", null,
                                   Enumerable.Empty<(int,int)>(),   // no chips for float add
                                   lines,
                                   isCashOut: false,
                                   dateTime: dateTime);
      if (!string.IsNullOrEmpty(batchId))
      {
        view.EnableDeletionForFloat(batchId);
      }
      view.Back += (_, __) => ShowTransactions();
      view.Deleted += (_, __) => { RefreshActivity(); ShowTransactions(); };
      ShowScreen(view);
    }

    private void ShowFoodSaleDetails(long saleId)
    {
      var view = new FoodSaleDetailsView(saleId);
      view.Back += (_, __) => ShowTransactions();
      ShowScreen(view);
    }

    private void ShowLostChipsBreakdown(string? batchId)
    {
      if (string.IsNullOrWhiteSpace(batchId))
      {
        MessageBox.Show(this, "Missing lost chips batch id.", "Details");
        return;
      }

      // Get date/time and notes
      var infoQuery = Database.Query(@"
SELECT datetime(time, 'localtime') AS formatted_time, notes
FROM activity_log
WHERE batch_id = $b
LIMIT 1", ("$b", batchId));
      
      string? dateTime = null;
      string? notes = null;
      
      if (infoQuery.Rows.Count > 0)
      {
        dateTime = infoQuery.Rows[0]["formatted_time"]?.ToString();
        var notesObj = infoQuery.Rows[0]["notes"];
        if (notesObj != null && notesObj != DBNull.Value)
        {
          notes = notesObj.ToString();
        }
      }

      // Get chip breakdown
      var dt = Database.Query(@"
SELECT denom_cents, delta_qty AS qty
FROM cashbox_movements
WHERE reason = 'LOST_CHIP' AND batch_id = $b
ORDER BY denom_cents DESC", ("$b", batchId));

      var lines = dt.Rows.Cast<DataRow>()
        .Select(r => (denom: Convert.ToInt32(r["denom_cents"]), qty: Convert.ToInt32(r["qty"])))
        .ToList();

      var view = new BreakdownView("Lost Chips (Tips)", notes,
                                   lines,   // Show as chips
                                   Enumerable.Empty<(int,int)>(),   // No cash
                                   isCashOut: false,
                                   dateTime: dateTime);
      if (!string.IsNullOrEmpty(batchId))
      {
        view.EnableDeletionForLostChips(batchId);
      }
      view.Back += (_, __) => ShowTransactions();
      view.Deleted += (_, __) => { RefreshActivity(); ShowTransactions(); };
      ShowScreen(view);
    }

  }
}
