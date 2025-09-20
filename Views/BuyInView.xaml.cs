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
  public partial class BuyInView : UserControl
  {
    public event EventHandler<BuyInConfirmedEventArgs>? Confirmed;
    public event EventHandler? Cancelled;

    // Australian currency denominations only
    private static readonly int[] CashDenoms = { 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };

    private Dictionary<int, int> _cashCounts = new();
    private Stack<int> _cashHistory = new();
    private int _currentMultiplier = 1;
    private double _buyInAmount = 0.0;
    private string _buyInAmountString = "0.00";

    public string MemberNumber => ScanBox.Text.Trim();
    public int TotalCents => _cashCounts.Sum(kv => kv.Key * kv.Value);
    public double TotalDollars => TotalCents / 100.0;
    public IReadOnlyDictionary<int, int> DenomCounts => _cashCounts;

    public BuyInView()
    {
      InitializeComponent();
      Loaded += (s, e) => { 
        InitializeCashInput(); 
        ScanBox.Focus(); 
      };
    }

    private void InitializeCashInput()
    {
      _cashCounts = CashDenoms.ToDictionary(d => d, d => 0);
      _currentMultiplier = 1;
      UpdateCashMultiplierDisplay();
      RefreshCashInput();
    }

    private void RefreshCashInput()
    {
      // Clear existing rows
      CashInputRowsPanel.Children.Clear();

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
          Text = _cashCounts[denom].ToString(),
          FontSize = 13,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(5)
        };
        Grid.SetColumn(countText, 1);

        var valueText = new TextBlock
        {
          Text = (_cashCounts[denom] * denom / 100.0).ToString("C", culture),
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
        CashInputRowsPanel.Children.Add(row);
      }

      UpdateCashButtonCounts();
      UpdateTotalCash();
    }

    private void UpdateTotalCash()
    {
      var culture = CultureInfo.GetCultureInfo("en-AU");
      double total = _cashCounts.Sum(kv => kv.Key * kv.Value) / 100.0;
      TotalCashText.Text = total.ToString("C", culture);
    }

    private void UpdateCashButtonCounts()
    {
      Cash5cCount.Text = _cashCounts[5].ToString();
      Cash10cCount.Text = _cashCounts[10].ToString();
      Cash20cCount.Text = _cashCounts[20].ToString();
      Cash50cCount.Text = _cashCounts[50].ToString();
      Cash1Count.Text = _cashCounts[100].ToString();
      Cash2Count.Text = _cashCounts[200].ToString();
      Cash5Count.Text = _cashCounts[500].ToString();
      Cash10Count.Text = _cashCounts[1000].ToString();
      Cash20Count.Text = _cashCounts[2000].ToString();
      Cash50Count.Text = _cashCounts[5000].ToString();
      Cash100Count.Text = _cashCounts[10000].ToString();
    }

    private string FormatDenom(int denom)
    {
      return denom switch
      {
        5 => "5c", 10 => "10c", 20 => "20c", 50 => "50c",
        100 => "$1", 200 => "$2", 500 => "$5", 1000 => "$10",
        2000 => "$20", 5000 => "$50", 10000 => "$100",
        _ => $"{denom}c"
      };
    }

    private void CashDenom_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button button && int.TryParse(button.Tag?.ToString(), out int denom))
      {
        _cashCounts[denom] += _currentMultiplier;
        // Add to history for undo functionality
        for (int i = 0; i < _currentMultiplier; i++)
        {
          _cashHistory.Push(denom);
        }
        RefreshCashInput();
      }
    }

    private void CashMultiplier_Click(object sender, RoutedEventArgs e)
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
      UpdateCashMultiplierDisplay();
    }

    private void UpdateCashMultiplierDisplay()
    {
      // Reset all to normal size and opacity
      Cash1xText.FontSize = 16;
      Cash1xText.Opacity = 0.6;
      Cash5xText.FontSize = 16;
      Cash5xText.Opacity = 0.6;
      Cash20xText.FontSize = 16;
      Cash20xText.Opacity = 0.6;

      // Highlight current multiplier
      switch (_currentMultiplier)
      {
        case 1:
          Cash1xText.FontSize = 24;
          Cash1xText.Opacity = 1.0;
          break;
        case 5:
          Cash5xText.FontSize = 24;
          Cash5xText.Opacity = 1.0;
          break;
        case 20:
          Cash20xText.FontSize = 24;
          Cash20xText.Opacity = 1.0;
          break;
      }
    }

    private void NextToAmount_Click(object sender, RoutedEventArgs e)
    {
      if (string.IsNullOrWhiteSpace(MemberNumber))
      {
        MessageBox.Show("Please scan or enter a member number first.");
        return;
      }
      if (TotalCents <= 0)
      {
        MessageBox.Show("Please add at least one coin/note.");
        return;
      }

      // Switch to amount input view
      CashInputView.Visibility = Visibility.Collapsed;
      AmountInputView.Visibility = Visibility.Visible;
      
      // Pre-fill buy-in amount with total cash
      _buyInAmount = TotalDollars;
      _buyInAmountString = _buyInAmount.ToString("F2");
      UpdateAmountDisplay();
      CalculateChange();
    }

    private void BackToCash_Click(object sender, RoutedEventArgs e)
    {
      // Switch back to cash input view
      AmountInputView.Visibility = Visibility.Collapsed;
      CashInputView.Visibility = Visibility.Visible;
    }

    private void Undo_Click(object sender, RoutedEventArgs e)
    {
      if (_cashHistory.Count > 0)
      {
        var lastDenom = _cashHistory.Pop();
        if (_cashCounts.ContainsKey(lastDenom) && _cashCounts[lastDenom] > 0)
        {
          _cashCounts[lastDenom]--;
          RefreshCashInput();
        }
      }
    }

    private void Clear_Click(object sender, RoutedEventArgs e)
    {
      _cashCounts.Clear();
      _cashHistory.Clear();
      foreach (var denom in CashDenoms)
      {
        _cashCounts[denom] = 0;
      }
      RefreshCashInput();
    }

    private void UpdateAmountDisplay()
    {
      var culture = CultureInfo.GetCultureInfo("en-AU");
      CashReceivedText.Text = TotalDollars.ToString("C", culture);
      BuyInAmountText.Text = _buyInAmount.ToString("C", culture);
    }

    private void Number_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button button && button.Tag is string digit)
      {
        if (digit == ".")
        {
          if (!_buyInAmountString.Contains("."))
          {
            _buyInAmountString += ".";
          }
        }
        else
        {
          if (_buyInAmountString == "0.00")
          {
            _buyInAmountString = digit;
          }
          else
          {
            _buyInAmountString += digit;
          }
        }

        if (double.TryParse(_buyInAmountString, out double amount))
        {
          _buyInAmount = amount;
          UpdateAmountDisplay();
          CalculateChange();
        }
      }
    }

    private void Backspace_Click(object sender, RoutedEventArgs e)
    {
      if (_buyInAmountString.Length > 1)
      {
        _buyInAmountString = _buyInAmountString.Substring(0, _buyInAmountString.Length - 1);
      }
      else
      {
        _buyInAmountString = "0.00";
      }

      if (double.TryParse(_buyInAmountString, out double amount))
      {
        _buyInAmount = amount;
        UpdateAmountDisplay();
        CalculateChange();
      }
    }

    private void ClearAmount_Click(object sender, RoutedEventArgs e)
    {
      _buyInAmount = 0.0;
      _buyInAmountString = "0.00";
      UpdateAmountDisplay();
      CalculateChange();
    }

    private void CalculateChange()
    {
      double change = TotalDollars - _buyInAmount;
      var culture = CultureInfo.GetCultureInfo("en-AU");
      ChangeText.Text = change.ToString("C", culture);

      // Calculate change breakdown using minimum change algorithm
      var changeBreakdown = CalculateMinimumChange(change);
      RefreshChangeBreakdown(changeBreakdown);
    }

    private Dictionary<int, int> CalculateMinimumChange(double changeAmount)
    {
      var breakdown = new Dictionary<int, int>();
      int remainingCents = (int)Math.Round(changeAmount * 100);

      // Australian currency denominations in descending order
      int[] denominations = { 10000, 5000, 2000, 1000, 500, 200, 100, 50, 20, 10, 5 };

      foreach (int denom in denominations)
      {
        if (remainingCents >= denom)
        {
          int count = remainingCents / denom;
          breakdown[denom] = count;
          remainingCents -= count * denom;
        }
      }

      return breakdown;
    }

    private void RefreshChangeBreakdown(Dictionary<int, int> changeBreakdown)
    {
      // Clear existing rows
      ChangeRowsPanel.Children.Clear();

      var culture = CultureInfo.GetCultureInfo("en-AU");

      // Create rows for each denomination that has change
      foreach (int denom in CashDenoms)
      {
        if (changeBreakdown.TryGetValue(denom, out int count) && count > 0)
        {
          var row = new Border
          {
            BorderBrush = new SolidColorBrush(Colors.Gray),
            BorderThickness = new Thickness(1, 0, 1, 1),
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
            Text = count.ToString(),
            FontSize = 13,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(5)
          };
          Grid.SetColumn(countText, 1);

          var valueText = new TextBlock
          {
            Text = (count * denom / 100.0).ToString("C", culture),
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
          ChangeRowsPanel.Children.Add(row);
        }
      }
    }

    private void ConfirmBuyIn_Click(object sender, RoutedEventArgs e)
    {
      if (_buyInAmount <= 0)
      {
        MessageBox.Show("Please enter a valid buy-in amount.");
        return;
      }

      if (_buyInAmount > TotalDollars)
      {
        MessageBox.Show("Buy-in amount cannot exceed cash received.");
        return;
      }

      // Calculate change breakdown for the actual transaction
      var changeBreakdown = CalculateMinimumChange(TotalDollars - _buyInAmount);
      
      // Pass both cash received AND buy-in amount
      Confirmed?.Invoke(this, new BuyInConfirmedEventArgs(
        MemberNumber, 
        new Dictionary<int, int>(_cashCounts), 
        (int)(_buyInAmount * 100),
        changeBreakdown
      ));
    }

    private void Back_Click(object sender, RoutedEventArgs e) => Cancelled?.Invoke(this, EventArgs.Empty);

    private void ClearScan_Click(object sender, RoutedEventArgs e) => ScanBox.Clear();

    private void ScanBox_KeyDown(object sender, KeyEventArgs e)
    {
      if (e.Key == Key.Enter && TotalCents > 0)
      {
        NextToAmount_Click(sender, e);
      }
    }
  }

  public sealed class BuyInConfirmedEventArgs : EventArgs
  {
    public string MemberNumber { get; }
    public Dictionary<int, int> CashReceived { get; }   // cash received from player
    public int BuyInAmountCents { get; }                // amount player is buying in for
    public Dictionary<int, int> ChangeBreakdown { get; } // change to give back

    public BuyInConfirmedEventArgs(string member,
                                 Dictionary<int, int> cashReceived,
                                 int buyInAmountCents,
                                 Dictionary<int, int> changeBreakdown)
      => (MemberNumber, CashReceived, BuyInAmountCents, ChangeBreakdown) = (member, cashReceived, buyInAmountCents, changeBreakdown);
  }
}