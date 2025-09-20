using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace pokersoc_connect.Views
{
  public partial class CashboxView : UserControl
  {
    // Australian currency denominations only (for cashbox)
    private static readonly int[] CashDenoms = { 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };

    private Dictionary<int, int> _floatCounts = new();
    private Stack<int> _floatHistory = new();
    private int _currentMultiplier = 1;

    public event EventHandler? CloseRequested;

    public CashboxView()
    {
      InitializeComponent();
      RefreshCashbox();
      InitializeFloatInput();
    }

    private void RefreshCashbox()
    {
      var counts = CashDenoms.ToDictionary(d => d, d => 0);
      var tipCounts = CashDenoms.ToDictionary(d => d, d => 0);
      string? err = null;

      try
      {
        if (Database.Conn != null)
        {
          // Get float amounts
          var floatDt = Database.Query("SELECT denom_cents, qty FROM cashbox_float");
          foreach (DataRow r in floatDt.Rows)
          {
            int d = Convert.ToInt32(r["denom_cents"]);
            int q = Convert.ToInt32(r["qty"]);
            if (counts.ContainsKey(d)) counts[d] = q;
          }

          // Get cashbox movements (excluding lost chips and float additions)
          var movesDt = Database.Query("SELECT denom_cents, SUM(delta_qty) AS net FROM cashbox_movements WHERE reason NOT IN ('LOST_CHIP', 'FLOAT_ADD') GROUP BY denom_cents");
          foreach (DataRow r in movesDt.Rows)
          {
            int d = Convert.ToInt32(r["denom_cents"]);
            int net = Convert.ToInt32(r["net"]);
            if (counts.ContainsKey(d)) counts[d] += net;
          }

          // Get tips (lost chips) - convert poker chip denominations to currency denominations
          var tipsDt = Database.Query("SELECT denom_cents, SUM(qty) AS qty FROM tips GROUP BY denom_cents");
          foreach (DataRow r in tipsDt.Rows)
          {
            int d = Convert.ToInt32(r["denom_cents"]);
            int q = Convert.ToInt32(r["qty"]);
            
            // Convert poker chip denominations to currency denominations for display
            var currencyDenoms = ConvertPokerChipToCurrencyDenoms(d, q);
            foreach (var (denom, count) in currencyDenoms)
            {
              if (tipCounts.ContainsKey(denom)) 
              {
                tipCounts[denom] += count;
              }
            }
          }
        }
      }
      catch (Exception ex)
      {
        err = ex.Message;
      }

      // Clear existing rows
      CashboxRowsPanel.Children.Clear();

      // Create rows for each denomination
      for (int i = 0; i < CashDenoms.Length; i++)
      {
        int denom = CashDenoms[i];
        bool isLastRow = (i == CashDenoms.Length - 1);
        
        var row = new Border
        {
          BorderBrush = new SolidColorBrush(Colors.Gray),
          BorderThickness = isLastRow ? new Thickness(1, 0, 1, 1) : new Thickness(1, 0, 1, 1),
          Height = 36
        };

        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(110) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(100) });

        var denominationText = new TextBlock
        {
          Text = FormatDenom(denom),
          FontSize = 13,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(denominationText, 0);

        var countText = new TextBlock
        {
          Text = counts[denom].ToString(),
          FontSize = 13,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(countText, 1);

        var valueText = new TextBlock
        {
          Text = (counts[denom] * denom / 100.0).ToString("C", CultureInfo.GetCultureInfo("en-AU")),
          FontSize = 13,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(valueText, 2);

        grid.Children.Add(denominationText);
        grid.Children.Add(countText);
        grid.Children.Add(valueText);
        row.Child = grid;
        CashboxRowsPanel.Children.Add(row);
      }

      double totalCash = counts.Sum(kv => kv.Key * kv.Value) / 100.0;
      double totalTips = tipCounts.Sum(kv => kv.Key * kv.Value) / 100.0;
      double grandTotal = totalCash + totalTips;

      var au = CultureInfo.GetCultureInfo("en-AU");
      
      // Calculate float amount (starting cash)
      double floatAmount = 0;
      try
      {
        if (Database.Conn != null)
        {
          var floatDt = Database.Query("SELECT denom_cents, qty FROM cashbox_float");
          foreach (DataRow r in floatDt.Rows)
          {
            int d = Convert.ToInt32(r["denom_cents"]);
            int q = Convert.ToInt32(r["qty"]);
            floatAmount += d * q / 100.0;
          }
        }
      }
      catch { }

      // Update individual text blocks
      TotalCashText.Text = totalCash.ToString("C", au);
      TipsText.Text = totalTips.ToString("C", au);
      FloatAmountText.Text = floatAmount.ToString("C", au);

      if (err != null)
      {
        TotalCashText.Text += $" (Error)";
        TipsText.Text += $" (Error)";
        FloatAmountText.Text += $" (Error)";
      }
    }

    private static string FormatDenom(int cents)
    {
      return cents < 100 ? $"{cents}¢" : $"${cents / 100}";
    }

    private static List<(int denom, int count)> ConvertPokerChipToCurrencyDenoms(int pokerChipDenom, int quantity)
    {
      // Convert poker chip denominations to Australian currency denominations
      // This breaks down poker chip values into actual currency denominations
      var result = new List<(int, int)>();
      
      switch (pokerChipDenom)
      {
        case 5: // 5c chip -> 5c coin
          result.Add((5, quantity));
          break;
        case 25: // 25c chip -> 20c + 5c coins
          result.Add((20, quantity)); // 20c coins
          result.Add((5, quantity));  // 5c coins
          break;
        case 100: // $1 chip -> $1 coin
          result.Add((100, quantity));
          break;
        case 200: // $2 chip -> $2 coin
          result.Add((200, quantity));
          break;
        case 500: // $5 chip -> $5 note
          result.Add((500, quantity));
          break;
        case 2500: // $25 chip -> $20 + $5 notes
          result.Add((2000, quantity)); // $20 notes
          result.Add((500, quantity));  // $5 notes
          break;
        case 10000: // $100 chip -> $100 note
          result.Add((10000, quantity));
          break;
        default:
          // If it's already a currency denomination, return as-is
          if (CashDenoms.Contains(pokerChipDenom))
          {
            result.Add((pokerChipDenom, quantity));
          }
          break;
      }
      
      return result;
    }

    private void AddFloat_Click(object sender, RoutedEventArgs e)
    {
      ShowFloatInputView();
    }

    private void BackToMain_Click(object sender, RoutedEventArgs e)
    {
      if (FloatInputView.Visibility == Visibility.Visible)
      {
        ShowMainCashboxView();
      }
      else
      {
        CloseRequested?.Invoke(this, EventArgs.Empty);
      }
    }

    private void ShowMainCashboxView()
    {
      MainCashboxView.Visibility = Visibility.Visible;
      FloatInputView.Visibility = Visibility.Collapsed;
    }

    private void ShowFloatInputView()
    {
      MainCashboxView.Visibility = Visibility.Collapsed;
      FloatInputView.Visibility = Visibility.Visible;
      RefreshFloatInput();
    }

    private void InitializeFloatInput()
    {
      _floatCounts = CashDenoms.ToDictionary(d => d, d => 0);
      _currentMultiplier = 1;
      UpdateFloatMultiplierDisplay();
      RefreshFloatInput();
    }

    private void RefreshFloatInput()
    {
      // Clear existing rows
      FloatInputRowsPanel.Children.Clear();

      var culture = CultureInfo.GetCultureInfo("en-AU");

      // Create rows for each denomination
      for (int i = 0; i < CashDenoms.Length; i++)
      {
        int denom = CashDenoms[i];
        bool isLastRow = (i == CashDenoms.Length - 1);
        
        var row = new Border
        {
          BorderBrush = new SolidColorBrush(Colors.Gray),
          BorderThickness = isLastRow ? new Thickness(1, 0, 1, 1) : new Thickness(1, 0, 1, 1),
          Height = 36
        };

        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(110) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(100) });

        var denominationText = new TextBlock
        {
          Text = FormatDenom(denom),
          FontSize = 13,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(denominationText, 0);

        var countText = new TextBlock
        {
          Text = _floatCounts[denom].ToString(),
          FontSize = 13,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(countText, 1);

        var valueText = new TextBlock
        {
          Text = (_floatCounts[denom] * denom / 100.0).ToString("C", culture),
          FontSize = 13,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(valueText, 2);

        grid.Children.Add(denominationText);
        grid.Children.Add(countText);
        grid.Children.Add(valueText);
        row.Child = grid;
        FloatInputRowsPanel.Children.Add(row);
      }

      UpdateFloatButtonCounts();
      UpdateTotalFloat();
    }

    private void UpdateTotalFloat()
    {
      var culture = CultureInfo.GetCultureInfo("en-AU");
      double total = _floatCounts.Sum(kv => kv.Key * kv.Value) / 100.0;
      TotalFloatText.Text = $"Total add float: {total.ToString("C", culture)}";
    }

    private void UpdateFloatButtonCounts()
    {
      Float5cCount.Text = _floatCounts[5].ToString();
      Float10cCount.Text = _floatCounts[10].ToString();
      Float20cCount.Text = _floatCounts[20].ToString();
      Float50cCount.Text = _floatCounts[50].ToString();
      Float1Count.Text = _floatCounts[100].ToString();
      Float2Count.Text = _floatCounts[200].ToString();
      Float5Count.Text = _floatCounts[500].ToString();
      Float10Count.Text = _floatCounts[1000].ToString();
      Float20Count.Text = _floatCounts[2000].ToString();
      Float50Count.Text = _floatCounts[5000].ToString();
      Float100Count.Text = _floatCounts[10000].ToString();
    }

    private void FloatDenom_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button button && int.TryParse(button.Tag?.ToString(), out int denom))
      {
        _floatCounts[denom] += _currentMultiplier;
        // Add to history for undo functionality
        for (int i = 0; i < _currentMultiplier; i++)
        {
          _floatHistory.Push(denom);
        }
        RefreshFloatInput();
      }
    }

    private void FloatMultiplier_Click(object sender, RoutedEventArgs e)
    {
      // Cycle through 1x, 5x, 20x
      _currentMultiplier = _currentMultiplier switch
      {
        1 => 5,
        5 => 20,
        20 => 1,
        _ => 1
      };

      // Update display to highlight current multiplier
      UpdateFloatMultiplierDisplay();
    }

    private void UpdateFloatMultiplierDisplay()
    {
      // Reset all to normal size and opacity
      Float1xText.FontSize = 16;
      Float1xText.Opacity = 0.6;
      Float5xText.FontSize = 16;
      Float5xText.Opacity = 0.6;
      Float20xText.FontSize = 16;
      Float20xText.Opacity = 0.6;

      // Highlight current multiplier
      switch (_currentMultiplier)
      {
        case 1:
          Float1xText.FontSize = 24;
          Float1xText.Opacity = 1.0;
          break;
        case 5:
          Float5xText.FontSize = 24;
          Float5xText.Opacity = 1.0;
          break;
        case 20:
          Float20xText.FontSize = 24;
          Float20xText.Opacity = 1.0;
          break;
      }
    }

    private void SaveFloat_Click(object sender, RoutedEventArgs e)
    {
      var totalFloatAmount = _floatCounts.Sum(kv => kv.Key * kv.Value) / 100.0;
      
      Database.InTransaction(tx =>
      {
        var batchId = Guid.NewGuid().ToString("N");
        foreach (var kv in _floatCounts)
        {
          if (kv.Value <= 0) continue;
          
          // Add to cashbox_movements
          Database.Exec(
            "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id, batch_id) " +
            "VALUES ($d, $q, 'FLOAT_ADD', NULL, NULL, $batch)",
            tx, ("$d", kv.Key), ("$q", kv.Value), ("$batch", batchId)
          );
          
          // Update cashbox_float table
          Database.Exec(
            "INSERT OR REPLACE INTO cashbox_float(denom_cents, qty) " +
            "VALUES ($d, COALESCE((SELECT qty FROM cashbox_float WHERE denom_cents = $d), 0) + $q)",
            tx, ("$d", kv.Key), ("$q", kv.Value)
          );
        }
        
        // Add to activity log
        Database.Exec(
          "INSERT INTO activity_log(activity_key, activity_type, activity_kind, method, staff, player_id, tx_id, batch_id, amount_cents, notes) " +
          "VALUES ($key, 'FLOAT_ADD', 'FLOAT_ADD', 'MANUAL', 'STAFF', NULL, NULL, $batch, $amount, $notes)",
          tx, 
          ("$key", Guid.NewGuid().ToString("N")),
          ("$batch", batchId),
          ("$amount", (int)(totalFloatAmount * 100)),
          ("$notes", $"Float added: {totalFloatAmount:C}")
        );
      });

      // Reset float counts
      _floatCounts = CashDenoms.ToDictionary(d => d, d => 0);
      RefreshFloatInput();
      ShowMainCashboxView();
      RefreshCashbox();
    }

    private void FloatUndo_Click(object sender, RoutedEventArgs e)
    {
      if (_floatHistory.Count > 0)
      {
        var lastDenom = _floatHistory.Pop();
        if (_floatCounts.ContainsKey(lastDenom) && _floatCounts[lastDenom] > 0)
        {
          _floatCounts[lastDenom]--;
          RefreshFloatInput();
        }
      }
    }

    private void FloatClear_Click(object sender, RoutedEventArgs e)
    {
      _floatCounts.Clear();
      _floatHistory.Clear();
      foreach (var denom in CashDenoms)
      {
        _floatCounts[denom] = 0;
      }
      RefreshFloatInput();
    }

    private sealed class CashboxRow
    {
      public string Denomination { get; set; } = "";
      public int    Count        { get; set; }
      public string Value        { get; set; } = "";
    }
  }
}
