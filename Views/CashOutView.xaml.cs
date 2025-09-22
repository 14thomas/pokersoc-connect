using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace pokersoc_connect.Views
{
  public partial class CashOutView : UserControl
  {
    public event EventHandler<CashOutConfirmedEventArgs>? Confirmed;
    public event EventHandler? Back;

    // Your chip set only
    private readonly Dictionary<int,int> _chipCounts = new()
      { {5,0},{10,0},{20,0},{25,0},{50,0},{100,0},{200,0},{500,0},{1000,0},{2000,0},{2500,0},{5000,0},{10000,0} };

    private readonly Stack<int> _history = new();
    private int _multiplier = 1;

    private readonly Dictionary<int,int> _available = new();
    private readonly Dictionary<int,int> _payout = new();
    private static readonly int[] DenomsDesc = { 10000,5000,2500,2000,1000,500,200,100,50,25,20,10,5 };

    // New fields for second screen functionality
    private readonly Dictionary<int,int> _changeDenominations = new();
    private readonly Dictionary<string,int> _selectedFoodItems = new();
    private double _tipAmount = 0.0;
    private double _foodTotal = 0.0;
    private double _extraCashAmount = 0.0;
    private readonly int[] _cashDenominations = { 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };

    public string MemberNumber => ScanBox.Text.Trim();
    public int TotalCents => _chipCounts.Sum(kv => kv.Key * kv.Value);
    public double TotalDollars => TotalCents / 100.0;
    public IReadOnlyDictionary<int,int> PayoutPlan => _payout;

    public CashOutView()
    {
      InitializeComponent();
      Loaded += (s,e) => { 
        LoadAvailableFromDb(); 
        UpdateAllBadges(); 
        RecomputePlan(); 
        UpdateMultiplierDisplay();
        ScanBox.Focus(); 
      };
    }

    private void LoadAvailableFromDb()
    {
      _available.Clear();

      var f = Database.Query("SELECT denom_cents, qty FROM cashbox_float");
      foreach (DataRow r in f.Rows) _available[Convert.ToInt32(r["denom_cents"])] = Convert.ToInt32(r["qty"]);

      var m = Database.Query("SELECT denom_cents, SUM(delta_qty) AS qty FROM cashbox_movements WHERE reason != 'FLOAT_ADD' GROUP BY denom_cents");
      foreach (DataRow r in m.Rows)
      {
        var d = Convert.ToInt32(r["denom_cents"]);
        var q = Convert.ToInt32(r["qty"]);
        _available[d] = _available.TryGetValue(d, out var cur) ? cur + q : q;
      }
      foreach (var d in DenomsDesc) if (!_available.ContainsKey(d)) _available[d] = 0;
    }

    private IEnumerable<Button> AllChipButtons() => new[] { C5,C25,C100,C200,C500,P2500,P10000 };

    private void UpdateBadgeFor(Button b, int count)
    {
      var badge = (FrameworkElement)b.Template.FindName("Badge", b);
      var text  = (TextBlock)b.Template.FindName("BadgeText", b);
      if (badge != null && text != null)
      {
        badge.Visibility = count > 0 ? Visibility.Visible : Visibility.Collapsed;
        if (count > 0) text.Text = count.ToString();
      }
    }

    private void UpdateAllBadges()
    {
      foreach (var b in AllChipButtons())
      {
        int cents = int.Parse(b.Tag.ToString()!);
        _chipCounts.TryGetValue(cents, out var c);
        UpdateBadgeFor(b, c);
        UpdateChipCountDisplay(cents, c);
      }
    }

    private void UpdateChipCountDisplay(int cents, int count)
    {
      switch (cents)
      {
        case 5: C5Count.Text = count.ToString(); break;
        case 25: C25Count.Text = count.ToString(); break;
        case 100: C100Count.Text = count.ToString(); break;
        case 200: C200Count.Text = count.ToString(); break;
        case 500: C500Count.Text = count.ToString(); break;
        case 2500: P2500Count.Text = count.ToString(); break;
        case 10000: P10000Count.Text = count.ToString(); break;
      }
    }

    private bool TryMakeChange(int amount, Dictionary<int,int> available, out Dictionary<int,int> plan)
    {
      var remaining = amount; plan = new();
      foreach (var d in DenomsDesc)
      {
        if (remaining <= 0) break;
        var want = remaining / d;
        if (want <= 0) continue;
        var take = Math.Min(want, available.TryGetValue(d, out var have) ? have : 0);
        if (take > 0) { plan[d] = take; remaining -= take * d; }
      }
      return remaining == 0;
    }

    private void RecomputePlan()
    {
      var culture = CultureInfo.GetCultureInfo("en-AU");
      TotalText.Text = (TotalDollars).ToString("C", culture);

      _payout.Clear();
      RefreshChipTable();
      
      if (TotalCents == 0)
      {
        NextBtn.IsEnabled = false; return;
      }

      var avail = _available.ToDictionary(kv => kv.Key, kv => kv.Value);
      if (TryMakeChange(TotalCents, avail, out var plan))
      {
        foreach (var kv in plan) _payout[kv.Key] = kv.Value;
        NextBtn.IsEnabled = true;
      }
      else
      {
        NextBtn.IsEnabled = false;
      }
    }

    private void RefreshChipTable()
    {
      var culture = CultureInfo.GetCultureInfo("en-AU");
      if (ChangeRowsPanel != null)
      ChangeRowsPanel.Children.Clear();
      if (TotalText != null)
        TotalText.Text = (TotalDollars).ToString("C", culture);

      // Debug: Check if we have any chips
      System.Diagnostics.Debug.WriteLine($"RefreshChipTable called. _chipCounts.Count = {_chipCounts.Count}");
      foreach (var kv in _chipCounts)
      {
        System.Diagnostics.Debug.WriteLine($"  {kv.Key}: {kv.Value}");
      }

      if (_chipCounts.Count == 0)
      {
        // Show empty state message
        var emptyRow = new Border
        {
          BorderBrush = Brushes.Gray,
          BorderThickness = new Thickness(1, 0, 1, 1),
          Background = Brushes.LightGray
        };

        var emptyGrid = new Grid { Height = 36 };
        emptyGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(110) });
        emptyGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
        emptyGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(100) });

        var emptyText = new TextBlock
        {
          Text = "No chips selected",
          FontSize = 14,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          FontStyle = FontStyles.Italic,
          Opacity = 0.7
        };
        Grid.SetColumn(emptyText, 0);
        Grid.SetColumnSpan(emptyText, 3);

        emptyGrid.Children.Add(emptyText);
        emptyRow.Child = emptyGrid;
        if (ChangeRowsPanel != null)
          ChangeRowsPanel.Children.Add(emptyRow);
        return;
      }

      // Show the selected chips (not the change calculation)
      var orderedChips = _chipCounts.OrderByDescending(kv => kv.Key).ToList();
      
      foreach (var kv in orderedChips)
      {
        if (kv.Value <= 0) continue;

        var row = new Border
        {
          BorderBrush = Brushes.Gray,
          BorderThickness = new Thickness(1, 0, 1, 1),
          Background = Brushes.White
        };

        var grid = new Grid { Height = 36 };
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(110) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(100) });

        var denomination = kv.Key switch
        {
          5 => "5c",
          25 => "25c", 
          100 => "$1",
          200 => "$2",
          500 => "$5",
          2500 => "$25",
          10000 => "$100",
          _ => $"{kv.Key}c"
        };

        var value = (kv.Key * kv.Value / 100.0).ToString("C", culture);

        var denomText = new TextBlock
        {
          Text = denomination,
          FontSize = 14,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(denomText, 0);

        var countText = new TextBlock
        {
          Text = kv.Value.ToString(),
          FontSize = 14,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(countText, 1);

        var valueText = new TextBlock
        {
          Text = value,
          FontSize = 14,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(valueText, 2);

        grid.Children.Add(denomText);
        grid.Children.Add(countText);
        grid.Children.Add(valueText);
        row.Child = grid;
        if (ChangeRowsPanel != null)
        ChangeRowsPanel.Children.Add(row);
      }
    }

    private void Multiplier_Click(object sender, RoutedEventArgs e)
    {
      // Cycle through 1x, 5x, 20x
      _multiplier = _multiplier switch
      {
        1 => 5,
        5 => 20,
        20 => 1,
        _ => 1
      };

      // Update display to highlight current multiplier
      UpdateMultiplierDisplay();
    }

    private void UpdateMultiplierDisplay()
    {
      // Reset all to normal size and opacity
      X1Text.FontSize = 16;
      X1Text.Opacity = 0.6;
      X5Text.FontSize = 16;
      X5Text.Opacity = 0.6;
      X20Text.FontSize = 16;
      X20Text.Opacity = 0.6;

      // Highlight current multiplier
      switch (_multiplier)
      {
        case 1:
          X1Text.FontSize = 24;
          X1Text.Opacity = 1.0;
          break;
        case 5:
          X5Text.FontSize = 24;
          X5Text.Opacity = 1.0;
          break;
        case 20:
          X20Text.FontSize = 24;
          X20Text.Opacity = 1.0;
          break;
      }
    }

    private void Chip_Click(object sender, RoutedEventArgs e)
    {
      if (sender is not Button b || !int.TryParse(b.Tag?.ToString(), out var cents)) return;
      var add = Math.Max(1, _multiplier);
      _chipCounts[cents] = _chipCounts.TryGetValue(cents, out var c) ? c + add : add;
      for (int i=0;i<add;i++) _history.Push(cents);
      UpdateBadgeFor(b, _chipCounts[cents]);
      UpdateChipCountDisplay(cents, _chipCounts[cents]);
      RecomputePlan();
    }

    private void Undo_Click(object sender, RoutedEventArgs e)
    {
      if (_history.Count == 0) return;
      var cents = _history.Pop();
      if (_chipCounts[cents] > 0) _chipCounts[cents]--;
      var btn = AllChipButtons().FirstOrDefault(x => x.Tag?.ToString()==cents.ToString());
      if (btn != null) 
      {
        UpdateBadgeFor(btn, _chipCounts[cents]);
        UpdateChipCountDisplay(cents, _chipCounts[cents]);
      }
      RecomputePlan();
    }

    private void Clear_Click(object sender, RoutedEventArgs e)
    {
      foreach (var k in _chipCounts.Keys.ToList()) _chipCounts[k] = 0;
      _history.Clear();
      UpdateAllBadges();
      RecomputePlan();
    }

    private void Back_Click(object sender, RoutedEventArgs e) => Back?.Invoke(this, EventArgs.Empty);
    private void ClearScan_Click(object sender, RoutedEventArgs e) => ScanBox.Clear();

    private void Next_Click(object sender, RoutedEventArgs e)
    {
      if (string.IsNullOrWhiteSpace(MemberNumber))
      {
        MessageBox.Show("Please scan or enter a member number first.");
        return;
      }
      if (_chipCounts.Sum(kv => kv.Value) == 0)
      {
        MessageBox.Show("Please select chips to cash out first.");
        return;
      }

      // Calculate initial change (chip value)
      var chipValueCents = TotalCents;
      CalculateOptimalChange(chipValueCents);
      
      // Load food items
      LoadFoodItems();
      
      // Initialize change customization
      InitializeChangeCustomization();
      
      // Update summary
      UpdateCashoutSummary();
      
      // Show second screen
      ShowChangeCustomizationScreen();
    }

    private void ConfirmCashOut_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        Database.InTransaction(transaction =>
        {
          // Add extra cash to cashbox if any
          if (_extraCashAmount > 0)
          {
            // Add extra cash as received money (break down into denominations)
            var extraCashCents = (int)(_extraCashAmount * 100);
            var denominations = new[] { 5000, 2000, 1000, 500, 200, 100, 50, 20, 10, 5 }; // $50, $20, $10, $5, $2, $1, 50c, 20c, 10c, 5c
            
            foreach (var denom in denominations)
            {
              if (extraCashCents >= denom)
              {
                var count = extraCashCents / denom;
                extraCashCents -= count * denom;
                
                Database.Exec("INSERT INTO cashbox_movements (denom_cents, delta_qty, reason, notes) VALUES (@denom, @qty, 'CASHOUT', @notes)", transaction,
                  ("@denom", denom),
                  ("@qty", count),
                  ("@notes", $"Extra cash from cashout - Member {MemberNumber}"));
              }
            }
          }

          // Add tip to tips table if any
          if (_tipAmount > 0)
          {
            // Break down tip into currency denominations for proper display in cashbox
            var tipCents = (int)(_tipAmount * 100);
            var denominations = new[] { 5000, 2000, 1000, 500, 200, 100, 50, 20, 10, 5 }; // $50, $20, $10, $5, $2, $1, 50c, 20c, 10c, 5c
            
            foreach (var denom in denominations)
            {
              if (tipCents >= denom)
              {
                var count = tipCents / denom;
                tipCents -= count * denom;
                
                Database.Exec("INSERT INTO tips (denom_cents, qty, notes) VALUES (@denom, @qty, @notes)", transaction,
                  ("@denom", denom),
                  ("@qty", count),
                  ("@notes", $"Tip from cashout - Member {MemberNumber}"));
              }
            }
          }

          // Food sales are included in the consolidated activity log entry in main window

          // Extra cash and other details are now handled in the main window's consolidated activity log
        });
      }
      catch (Exception ex)
      {
        System.Diagnostics.Debug.WriteLine($"Error processing cashout: {ex.Message}");
      }

      // Final confirmation - process the cashout with all customizations
      Confirmed?.Invoke(this, new CashOutConfirmedEventArgs(
        MemberNumber,
        new Dictionary<int,int>(_changeDenominations),   // final change plan
        new Dictionary<int,int>(_chipCounts),            // chips turned in
        TotalCents,
        _foodTotal,
        _tipAmount,
        _extraCashAmount));
    }

    private void BackToChipSelection_Click(object sender, RoutedEventArgs e)
    {
      ShowChipSelectionScreen();
    }

    private void ScanBox_KeyDown(object sender, KeyEventArgs e)
    {
      if (e.Key == Key.Enter && NextBtn.IsEnabled) Next_Click(sender, e);
    }

    // New methods for second screen functionality
    private void ShowChipSelectionScreen()
    {
      ChipSelectionScreen.Visibility = Visibility.Visible;
      ChangeCustomizationScreen.Visibility = Visibility.Collapsed;
    }

    private void ShowChangeCustomizationScreen()
    {
      ChipSelectionScreen.Visibility = Visibility.Collapsed;
      ChangeCustomizationScreen.Visibility = Visibility.Visible;
      
      // Initialize values for second screen
      _tipAmount = 0.0;
      _foodTotal = 0.0;
      _extraCashAmount = 0.0;
      
      // Initialize change customization with optimal change
      CalculateOptimalChange(TotalCents);
      RefreshChangeTable(); // Show the change breakdown on second screen
      UpdateCashoutSummary();
    }

    private void RefreshChangeTable()
    {
      var culture = CultureInfo.GetCultureInfo("en-AU");
      if (ChangeCustomizationRowsPanel != null)
        ChangeCustomizationRowsPanel.Children.Clear();
      if (ChangeTotalText != null)
        ChangeTotalText.Text = (_changeDenominations.Sum(kv => kv.Key * kv.Value) / 100.0).ToString("C", culture);

      if (_changeDenominations.Count == 0)
      {
        // Show empty state message
        var emptyRow = new Border
        {
          BorderBrush = Brushes.Gray,
          BorderThickness = new Thickness(1, 0, 1, 1),
          Background = Brushes.LightGray
        };

        var emptyGrid = new Grid { Height = 36 };
        emptyGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(110) });
        emptyGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
        emptyGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(100) });

        var emptyText = new TextBlock
        {
          Text = "No change calculated",
          FontSize = 14,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          FontStyle = FontStyles.Italic,
          Opacity = 0.7
        };
        Grid.SetColumn(emptyText, 0);
        Grid.SetColumnSpan(emptyText, 3);

        emptyGrid.Children.Add(emptyText);
        emptyRow.Child = emptyGrid;
        if (ChangeCustomizationRowsPanel != null)
          ChangeCustomizationRowsPanel.Children.Add(emptyRow);
        return;
      }

      // Show the change denominations
      var orderedChange = _changeDenominations.OrderByDescending(kv => kv.Key).ToList();
      
      foreach (var kv in orderedChange)
      {
        if (kv.Value <= 0) continue;

        var row = new Border
        {
          BorderBrush = Brushes.Gray,
          BorderThickness = new Thickness(1, 0, 1, 1),
          Background = Brushes.White
        };

        var grid = new Grid { Height = 36 };
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(110) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(100) });

        var denomination = kv.Key switch
        {
          5 => "5c",
          10 => "10c",
          20 => "20c",
          50 => "50c",
          100 => "$1",
          200 => "$2",
          500 => "$5",
          1000 => "$10",
          2000 => "$20",
          5000 => "$50",
          _ => $"{kv.Key}c"
        };

        var value = (kv.Key * kv.Value / 100.0).ToString("C", culture);

        var denomText = new TextBlock
        {
          Text = denomination,
          FontSize = 14,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(denomText, 0);

        var countText = new TextBlock
        {
          Text = kv.Value.ToString(),
          FontSize = 14,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(countText, 1);

        var valueText = new TextBlock
        {
          Text = value,
          FontSize = 14,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(valueText, 2);

        grid.Children.Add(denomText);
        grid.Children.Add(countText);
        grid.Children.Add(valueText);
        row.Child = grid;
        if (ChangeCustomizationRowsPanel != null)
          ChangeCustomizationRowsPanel.Children.Add(row);
      }
    }

    private void CalculateOptimalChange(int totalCents)
    {
      _changeDenominations.Clear();
      
      // Round to nearest 5c (Australian standard)
      var roundedCents = ((totalCents + 2) / 5) * 5;
      var remaining = roundedCents;

      foreach (var denom in _cashDenominations.OrderByDescending(d => d))
      {
        var count = remaining / denom;
        if (count > 0)
        {
          _changeDenominations[denom] = count;
          remaining -= count * denom;
        }
      }
      
      // If there's still remaining (shouldn't happen with proper rounding), log it
      if (remaining > 0)
      {
        System.Diagnostics.Debug.WriteLine($"Warning: {remaining} cents remaining after rounding {totalCents} to {roundedCents} cents");
      }
    }

    private void LoadFoodItems()
    {
      // Food items will be loaded via dialog when Add Food button is clicked
      _selectedFoodItems.Clear();
      _foodTotal = 0.0;
    }

    private void InitializeChangeCustomization()
    {
      // Change customization will be handled via dialog when Change Denominations button is clicked
      CalculateOptimalChange(TotalCents);
    }

    private void UpdateChangeDisplay()
    {
      // Change display will be updated via the summary panel
      UpdateCashoutSummary();
    }

    private void UpdateCashoutSummary()
    {
      var chipValue = TotalDollars;
      var foodValue = _foodTotal;
      var tipValue = _tipAmount;
      var extraCashValue = _extraCashAmount;
      
      // Total amount player should receive = chip value - food - tip + extra cash
      // Extra cash INCREASES the change needed (player pays extra cash, gets more change)
      var totalCashoutValue = chipValue - foodValue - tipValue + extraCashValue;
      var changeValue = _changeDenominations.Sum(kv => kv.Key * kv.Value) / 100.0;

      if (ChipValueText != null)
        ChipValueText.Text = $"Chip Value: {chipValue:C}";
      if (FoodValueText != null)
        FoodValueText.Text = $"Food: {foodValue:C}";
      if (TipValueText != null)
        TipValueText.Text = $"Tip: {tipValue:C}";
      if (ExtraCashValueText != null)
        ExtraCashValueText.Text = $"Extra Cash: {extraCashValue:C}";
      if (FinalChangeText != null)
        FinalChangeText.Text = $"Final Change: {totalCashoutValue:C}";

      // Enable confirm button only if we have chips selected
      if (ConfirmCashOutBtn != null)
        ConfirmCashOutBtn.IsEnabled = TotalCents > 0;
        
      // Auto-update change if not manually customized and tip/food changed
      if (!_isManualChangeCustomization)
      {
        CalculateOptimalChange((int)(totalCashoutValue * 100));
        RefreshChangeTable();
      }
    }


    private static string FormatDenom(int cents)
    {
      return cents < 100 ? $"{cents}¢" : $"${cents / 100}";
    }


    // New button click methods for the 8-button layout
    private void AddFood_Click(object sender, RoutedEventArgs e)
    {
      // Show inline food selection dialog
      PopulateFoodItemsList();
      FoodSelectionDialog.Visibility = Visibility.Visible;
    }

    private void AddTip_Click(object sender, RoutedEventArgs e)
    {
      // Show inline tip input dialog with keypad
      if (_tipAmount == 0.0)
      {
        // First time opening: initialize with coin sum (5c, 10c, 20c, 50c, $1, $2)
        var coinDenominations = new[] { 5, 10, 20, 50, 100, 200 }; // Only coins
        var coinChangeAmount = _changeDenominations
          .Where(kv => coinDenominations.Contains(kv.Key))
          .Sum(kv => kv.Key * kv.Value) / 100.0;
        _tipInputValue = coinChangeAmount;
      }
      else
      {
        // Reopening: initialize with current tip amount
        _tipInputValue = _tipAmount;
      }
      
      UpdateTipDisplay();
      TipInputDialog.Visibility = Visibility.Visible;
    }

    private void ChangeDenominations_Click(object sender, RoutedEventArgs e)
    {
      // Show inline change denominations dialog
      PopulateChangeDenominationsList();
      ChangeDenominationsDialog.Visibility = Visibility.Visible;
    }

    private void AddExtraCash_Click(object sender, RoutedEventArgs e)
    {
      // Show inline extra cash input dialog with denomination buttons
      _extraCashInputValue = _extraCashAmount;
      UpdateExtraCashDisplay();
      ExtraCashInputDialog.Visibility = Visibility.Visible;
    }

    // Inline dialog methods
    private void PopulateFoodItemsList()
    {
      if (FoodItemsList == null) return;
      
      FoodItemsList.Children.Clear();
      
      try
      {
        if (Database.Conn != null)
        {
          // Get actual food items from database
          var products = Database.Query("SELECT product_id, name, price FROM products ORDER BY name");
          
          foreach (DataRow row in products.Rows)
          {
            var name = row["name"].ToString();
            var price = Convert.ToDouble(row["price"]);
            var productId = Convert.ToInt32(row["product_id"]);
            
            var button = new Button
            {
              Content = $"{name} - {price:C}",
              Tag = new { Id = productId, Price = price, Name = name },
              Margin = new Thickness(2),
              Padding = new Thickness(10, 8, 10, 8),
              Background = Brushes.LightBlue,
              HorizontalAlignment = HorizontalAlignment.Stretch
            };
            button.Click += FoodItemButton_Click;
            
            FoodItemsList.Children.Add(button);
          }
          
          // If no products found, add sample items
          if (FoodItemsList.Children.Count == 0)
          {
            var sampleItems = new[]
            {
              new { Name = "Chips", Price = 3.50 },
              new { Name = "Drink", Price = 2.00 },
              new { Name = "Sandwich", Price = 8.50 },
              new { Name = "Coffee", Price = 4.00 },
              new { Name = "Snack Bar", Price = 2.50 }
            };
            
            foreach (var item in sampleItems)
            {
              var button = new Button
              {
                Content = $"{item.Name} - {item.Price:C}",
                Tag = new { Id = 0, Price = item.Price, Name = item.Name },
                Margin = new Thickness(2),
                Padding = new Thickness(10, 8, 10, 8),
                Background = Brushes.LightBlue,
                HorizontalAlignment = HorizontalAlignment.Stretch
              };
              button.Click += FoodItemButton_Click;
              
              FoodItemsList.Children.Add(button);
            }
          }
        }
      }
      catch { }
      
      if (FoodItemsList.Children.Count == 0)
      {
        FoodItemsList.Children.Add(new TextBlock 
        { 
          Text = "No food items available", 
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(10)
        });
      }
      
      // Update selected items display
      UpdateSelectedFoodItemsDisplay();
    }

    private void FoodItemButton_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button button && button.Tag is { } tag)
      {
        // Cast tag to dynamic to access properties
        dynamic item = tag;
        var itemKey = $"{item.Name}_{item.Price}";
        
        if (_selectedFoodItems.ContainsKey(itemKey))
        {
          _selectedFoodItems[itemKey]++;
        }
        else
        {
          _selectedFoodItems[itemKey] = 1;
        }
        
        // Update display
        UpdateSelectedFoodItemsDisplay();
      }
    }

    private void UpdateSelectedFoodItemsDisplay()
    {
      if (SelectedFoodItemsList == null || FoodTotalText == null) return;
      
      SelectedFoodItemsList.Children.Clear();
      
      _foodTotal = 0.0;
      
      foreach (var kvp in _selectedFoodItems)
      {
        var parts = kvp.Key.Split(new char[] { '_' });
        var name = parts[0];
        var price = double.Parse(parts[1]);
        var quantity = kvp.Value;
        var total = price * quantity;
        
        _foodTotal += total;
        
        var itemPanel = new StackPanel
        {
          Orientation = Orientation.Horizontal,
          Margin = new Thickness(0, 2, 0, 2)
        };
        
        var nameText = new TextBlock
        {
          Text = $"{name} x{quantity}",
          Width = 200,
          VerticalAlignment = VerticalAlignment.Center
        };
        
        var priceText = new TextBlock
        {
          Text = total.ToString("C"),
          Width = 80,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Right
        };
        
        var removeButton = new Button
        {
          Content = "✕",
          Width = 30,
          Height = 25,
          Margin = new Thickness(5, 0, 0, 0),
          Background = Brushes.LightCoral,
          FontWeight = FontWeights.Bold
        };
        removeButton.Click += (s, e) => RemoveFoodItem(kvp.Key);
        
        itemPanel.Children.Add(nameText);
        itemPanel.Children.Add(priceText);
        itemPanel.Children.Add(removeButton);
        
        SelectedFoodItemsList.Children.Add(itemPanel);
      }
      
      FoodTotalText.Text = $"Total: {_foodTotal:C}";
    }
    
    private void RemoveFoodItem(string itemKey)
    {
      if (_selectedFoodItems.ContainsKey(itemKey))
      {
        _selectedFoodItems[itemKey]--;
        if (_selectedFoodItems[itemKey] <= 0)
        {
          _selectedFoodItems.Remove(itemKey);
        }
        UpdateSelectedFoodItemsDisplay();
      }
    }

    private void CancelFoodSelection_Click(object sender, RoutedEventArgs e)
    {
      FoodSelectionDialog.Visibility = Visibility.Collapsed;
      _selectedFoodItems.Clear();
      _foodTotal = 0.0;
    }

    private void ConfirmFoodSelection_Click(object sender, RoutedEventArgs e)
    {
      // Food total is already calculated in UpdateSelectedFoodItemsDisplay
      FoodSelectionDialog.Visibility = Visibility.Collapsed;
      UpdateCashoutSummary();
    }

    private void TipKeypadButton_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button button && button.Tag is string tag)
      {
        switch (tag)
        {
          case "backspace":
            // Convert to cents, remove last digit, convert back
            var cents = (int)(_tipInputValue * 100);
            cents = cents / 10; // Remove last digit
            _tipInputValue = cents / 100.0;
            break;
          
          case ".":
            // Handle decimal point - already handled by the digit input logic
            break;
          
          default:
            if (double.TryParse(tag, out double digit))
            {
              // Convert to cents, add digit, convert back
              var totalCents = (int)(_tipInputValue * 100);
              totalCents = totalCents * 10 + (int)digit;
              _tipInputValue = totalCents / 100.0;
            }
            break;
        }
        
        UpdateTipDisplay();
      }
    }

    private void UpdateTipDisplay()
    {
      if (TipAmountDisplay != null)
      {
        TipAmountDisplay.Text = _tipInputValue.ToString("C");
      }
    }

    private void CloseTipInput_Click(object sender, RoutedEventArgs e)
    {
      TipInputDialog.Visibility = Visibility.Collapsed;
      // Don't change _tipAmount - it stays at current value
    }

    private void SetTipInput_Click(object sender, RoutedEventArgs e)
    {
      // Update tip amount when user sets it
      _tipAmount = _tipInputValue;
      UpdateCashoutSummary();
      // Keep dialog open for persistent editing
    }

    private void ExtraCashButton_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button button && button.Tag is string tag)
      {
        switch (tag)
        {
          case "clear":
            _extraCashInputValue = 0.0;
            break;
          
          case "backspace":
            // Convert to cents, remove last digit, convert back
            var cents = (int)(_extraCashInputValue * 100);
            cents = cents / 10; // Remove last digit
            _extraCashInputValue = cents / 100.0;
            break;
          
          default:
            if (int.TryParse(tag, out int denominationCents))
            {
              _extraCashInputValue += denominationCents / 100.0;
            }
            break;
        }
        
        UpdateExtraCashDisplay();
      }
    }

    private void UpdateExtraCashDisplay()
    {
      if (ExtraCashAmountDisplay != null)
      {
        ExtraCashAmountDisplay.Text = _extraCashInputValue.ToString("C");
      }
    }

    private void CloseExtraCashInput_Click(object sender, RoutedEventArgs e)
    {
      ExtraCashInputDialog.Visibility = Visibility.Collapsed;
    }

    private void ConfirmExtraCashInput_Click(object sender, RoutedEventArgs e)
    {
      _extraCashAmount = _extraCashInputValue;
      UpdateCashoutSummary();
      ExtraCashInputDialog.Visibility = Visibility.Collapsed;
      _extraCashInputValue = 0.0;
    }

    private void PopulateChangeDenominationsList()
    {
      if (ChangeDenominationsList == null) return;
      
      ChangeDenominationsList.Children.Clear();
      
      var culture = CultureInfo.GetCultureInfo("en-AU");
      var currentChange = _changeDenominations.Sum(kv => kv.Key * kv.Value) / 100.0;
      
      var headerText = new TextBlock
      {
        Text = $"Current change total: {currentChange:C}",
        FontSize = 16,
        FontWeight = FontWeights.Bold,
        Margin = new Thickness(0, 0, 0, 10),
        HorizontalAlignment = HorizontalAlignment.Center
      };
      ChangeDenominationsList.Children.Add(headerText);
      
      // Add all possible denominations, not just the ones with values
      var allDenominations = new[] { 5000, 2000, 1000, 500, 200, 100, 50, 20, 10, 5 };
      
      foreach (var denomCents in allDenominations)
      {
        var currentCount = _changeDenominations.TryGetValue(denomCents, out var count) ? count : 0;
        
        var denom = denomCents switch
        {
          5 => "5c",
          10 => "10c",
          20 => "20c",
          50 => "50c",
          100 => "$1",
          200 => "$2",
          500 => "$5",
          1000 => "$10",
          2000 => "$20",
          5000 => "$50",
          _ => $"{denomCents}c"
        };
        
        var row = new Border
        {
          BorderBrush = Brushes.Gray,
          BorderThickness = new Thickness(1),
          Background = Brushes.White,
          Margin = new Thickness(2)
        };
        
        var grid = new Grid { Height = 40 };
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(40) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(60) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(40) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
        
        // Denomination label
        var denomText = new TextBlock
        {
          Text = denom,
          FontSize = 14,
          FontWeight = FontWeights.Bold,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center
        };
        Grid.SetColumn(denomText, 0);
        
        // Minus button
        var minusButton = new Button
        {
          Content = "-",
          FontSize = 16,
          FontWeight = FontWeights.Bold,
          Background = Brushes.LightCoral,
          Tag = new { Denom = denomCents, Action = "minus" }
        };
        minusButton.Click += ChangeDenominationButton_Click;
        Grid.SetColumn(minusButton, 1);
        
        // Count display
        var countText = new TextBlock
        {
          Text = currentCount.ToString(),
          FontSize = 14,
          FontWeight = FontWeights.Bold,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center
        };
        Grid.SetColumn(countText, 2);
        
        // Plus button
        var plusButton = new Button
        {
          Content = "+",
          FontSize = 16,
          FontWeight = FontWeights.Bold,
          Background = Brushes.LightGreen,
          Tag = new { Denom = denomCents, Action = "plus" }
        };
        plusButton.Click += ChangeDenominationButton_Click;
        Grid.SetColumn(plusButton, 3);
        
        // Value display
        var valueText = new TextBlock
        {
          Text = (denomCents * currentCount / 100.0).ToString("C", culture),
          FontSize = 12,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center
        };
        Grid.SetColumn(valueText, 4);
        
        grid.Children.Add(denomText);
        grid.Children.Add(minusButton);
        grid.Children.Add(countText);
        grid.Children.Add(plusButton);
        grid.Children.Add(valueText);
        row.Child = grid;
        
        ChangeDenominationsList.Children.Add(row);
      }
    }

    private void CloseChangeDenominations_Click(object sender, RoutedEventArgs e)
    {
      ChangeDenominationsDialog.Visibility = Visibility.Collapsed;
    }

    private void ChangeDenominationButton_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button button && button.Tag is { } tag)
      {
        var denomCents = (int)tag.GetType().GetProperty("Denom")?.GetValue(tag)!;
        var action = (string)tag.GetType().GetProperty("Action")?.GetValue(tag)!;
        
        var currentCount = _changeDenominations.TryGetValue(denomCents, out var count) ? count : 0;
        
        switch (action)
        {
          case "plus":
            _changeDenominations[denomCents] = currentCount + 1;
            break;
          case "minus":
            if (currentCount > 0)
            {
              _changeDenominations[denomCents] = currentCount - 1;
              if (_changeDenominations[denomCents] == 0)
              {
                _changeDenominations.Remove(denomCents);
              }
            }
            break;
        }
        
        // Mark as manually customized
        _isManualChangeCustomization = true;
        
        // Refresh displays
        RefreshChangeTable();
        UpdateCashoutSummary();
        PopulateChangeDenominationsList();
      }
    }

    private void ResetToOptimalChange_Click(object sender, RoutedEventArgs e)
    {
      _isManualChangeCustomization = false;
      
      var chipValue = TotalDollars;
      var foodValue = _foodTotal;
      var tipValue = _tipAmount;
      var extraCashValue = _extraCashAmount;
      var totalCashoutValue = chipValue + extraCashValue - foodValue - tipValue;
      
      CalculateOptimalChange((int)(totalCashoutValue * 100));
      RefreshChangeTable();
      UpdateCashoutSummary();
      PopulateChangeDenominationsList();
    }

    // Fields to store tip input
    private double _tipInputValue = 0.0;
    private double _extraCashInputValue = 0.0;
    private bool _isManualChangeCustomization = false;
  }

  public sealed class CashOutConfirmedEventArgs : EventArgs
  {
    public string MemberNumber { get; }
    public Dictionary<int,int> PayoutPlan { get; }   // cash paid out (AU denoms)
    public Dictionary<int,int> ChipsIn { get; }      // chips turned in
    public int TotalCents { get; }
    public double FoodTotal { get; }
    public double TipAmount { get; }
    public double ExtraCashAmount { get; }
    
    public CashOutConfirmedEventArgs(string member,
                                     Dictionary<int,int> plan,
                                     Dictionary<int,int> chipsIn,
                                     int totalCents,
                                     double foodTotal,
                                     double tipAmount,
                                     double extraCashAmount)
      => (MemberNumber, PayoutPlan, ChipsIn, TotalCents, FoodTotal, TipAmount, ExtraCashAmount) = 
         (member, plan, chipsIn, totalCents, foodTotal, tipAmount, extraCashAmount);
  }
}
