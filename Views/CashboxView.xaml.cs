using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace pokersoc_connect.Views
{
  public partial class CashboxView : UserControl
  {
    // Australian currency denominations only (for cashbox)
    private static readonly int[] CashDenoms = { 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };

    private Dictionary<int, int> _floatCounts = new();
    private Stack<int> _floatHistory = new();
    private int _currentMultiplier = 1;

    private Dictionary<int, int> _addTipCounts = new();
    private Stack<int> _addTipHistory = new();
    private int _addTipMultiplier = 1;

    public event EventHandler? CloseRequested;

    public CashboxView()
    {
      InitializeComponent();
      RefreshCashbox();
      InitializeFloatInput();
      UpdateTotalSalesDisplay();
      LoadCurrencyImages();
    }

    private void LoadCurrencyImages()
    {
      string currencyPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Assets", "Currency");
      
      // Load coin images
      LoadCurrencyImage(Float5cImage, Float5cText, Path.Combine(currencyPath, "coin_5c.png"));
      LoadCurrencyImage(Float10cImage, Float10cText, Path.Combine(currencyPath, "coin_10c.png"));
      LoadCurrencyImage(Float20cImage, Float20cText, Path.Combine(currencyPath, "coin_20c.png"));
      LoadCurrencyImage(Float50cImage, Float50cText, Path.Combine(currencyPath, "coin_50c.png"));
      LoadCurrencyImage(Float1Image, Float1Text, Path.Combine(currencyPath, "coin_1.png"));
      LoadCurrencyImage(Float2Image, Float2Text, Path.Combine(currencyPath, "coin_2.png"));
      
      // Load note images
      LoadCurrencyImage(Float5Image, Float5Text, Path.Combine(currencyPath, "note_5.png"));
      LoadCurrencyImage(Float10Image, Float10Text, Path.Combine(currencyPath, "note_10.png"));
      LoadCurrencyImage(Float20Image, Float20Text, Path.Combine(currencyPath, "note_20.png"));
      LoadCurrencyImage(Float50Image, Float50Text, Path.Combine(currencyPath, "note_50.png"));
      LoadCurrencyImage(Float100Image, Float100Text, Path.Combine(currencyPath, "note_100.png"));

      // Add Tip view currency images (reuse same paths)
      LoadCurrencyImage(AddTip5cImage, null, Path.Combine(currencyPath, "coin_5c.png"));
      LoadCurrencyImage(AddTip10cImage, null, Path.Combine(currencyPath, "coin_10c.png"));
      LoadCurrencyImage(AddTip20cImage, null, Path.Combine(currencyPath, "coin_20c.png"));
      LoadCurrencyImage(AddTip50cImage, null, Path.Combine(currencyPath, "coin_50c.png"));
      LoadCurrencyImage(AddTip1Image, null, Path.Combine(currencyPath, "coin_1.png"));
      LoadCurrencyImage(AddTip2Image, null, Path.Combine(currencyPath, "coin_2.png"));
      LoadCurrencyImage(AddTip5Image, null, Path.Combine(currencyPath, "note_5.png"));
      LoadCurrencyImage(AddTip10Image, null, Path.Combine(currencyPath, "note_10.png"));
      LoadCurrencyImage(AddTip20Image, null, Path.Combine(currencyPath, "note_20.png"));
      LoadCurrencyImage(AddTip50Image, null, Path.Combine(currencyPath, "note_50.png"));
      LoadCurrencyImage(AddTip100Image, null, Path.Combine(currencyPath, "note_100.png"));
    }

    private void LoadCurrencyImage(Image imageControl, TextBlock? fallbackText, string imagePath)
    {
      try
      {
        if (File.Exists(imagePath))
        {
          var bitmap = new BitmapImage();
          bitmap.BeginInit();
          bitmap.UriSource = new Uri(imagePath, UriKind.Absolute);
          bitmap.CacheOption = BitmapCacheOption.OnLoad;
          bitmap.EndInit();
          imageControl.Source = bitmap;
          if (fallbackText != null) fallbackText.Visibility = Visibility.Collapsed;
        }
        else if (fallbackText != null)
        {
          fallbackText.Visibility = Visibility.Visible;
        }
      }
      catch
      {
        if (fallbackText != null) fallbackText.Visibility = Visibility.Visible;
      }
    }

    public void RefreshCashbox()
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
      
      // Update total sales display
      UpdateTotalSalesDisplay();
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

    private void AddTip_Click(object sender, RoutedEventArgs e)
    {
      ShowAddTipView();
    }

    private void ShowAddTipView()
    {
      MainCashboxView.Visibility = Visibility.Collapsed;
      FloatInputView.Visibility = Visibility.Collapsed;
      ExchangeView.Visibility = Visibility.Collapsed;
      AddTipView.Visibility = Visibility.Visible;
      _addTipCounts = CashDenoms.ToDictionary(d => d, d => 0);
      _addTipHistory.Clear();
      _addTipMultiplier = 1;
      UpdateAddTipMultiplierDisplay();
      RefreshAddTipInput();
    }

    private void AddTipDenom_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button button && int.TryParse(button.Tag?.ToString(), out int denom))
      {
        _addTipCounts[denom] += _addTipMultiplier;
        for (int i = 0; i < _addTipMultiplier; i++) _addTipHistory.Push(denom);
        RefreshAddTipInput();
      }
    }

    private void AddTipMultiplier1x_Click(object sender, RoutedEventArgs e)
    {
      _addTipMultiplier = 1;
      UpdateAddTipMultiplierDisplay();
    }

    private void AddTipMultiplier5x_Click(object sender, RoutedEventArgs e)
    {
      _addTipMultiplier = 5;
      UpdateAddTipMultiplierDisplay();
    }

    private void AddTipMultiplier20x_Click(object sender, RoutedEventArgs e)
    {
      _addTipMultiplier = 20;
      UpdateAddTipMultiplierDisplay();
    }

    private void UpdateAddTipMultiplierDisplay()
    {
      AddTip1xButton.Background = Brushes.LightBlue;
      AddTip5xButton.Background = Brushes.LightBlue;
      AddTip20xButton.Background = Brushes.LightBlue;
      switch (_addTipMultiplier)
      {
        case 1: AddTip1xButton.Background = Brushes.LightGreen; break;
        case 5: AddTip5xButton.Background = Brushes.LightGreen; break;
        case 20: AddTip20xButton.Background = Brushes.LightGreen; break;
      }
    }

    private void AddTipClear_Click(object sender, RoutedEventArgs e)
    {
      _addTipCounts = CashDenoms.ToDictionary(d => d, d => 0);
      _addTipHistory.Clear();
      RefreshAddTipInput();
    }

    private void AddTipUndo_Click(object sender, RoutedEventArgs e)
    {
      if (_addTipHistory.Count > 0)
      {
        var lastDenom = _addTipHistory.Pop();
        if (_addTipCounts.ContainsKey(lastDenom) && _addTipCounts[lastDenom] > 0)
        {
          _addTipCounts[lastDenom]--;
          RefreshAddTipInput();
        }
      }
    }

    private void RefreshAddTipInput()
    {
      AddTipRowsPanel.Items.Clear();
      var culture = CultureInfo.GetCultureInfo("en-AU");
      var addedDenoms = CashDenoms.Where(d => _addTipCounts.ContainsKey(d) && _addTipCounts[d] > 0).ToList();

      if (addedDenoms.Count == 0)
      {
        var placeholder = new Border
        {
          BorderBrush = new SolidColorBrush(Colors.Gray),
          BorderThickness = new Thickness(1, 0, 1, 1),
          Background = new SolidColorBrush(Color.FromRgb(250, 250, 250))
        };
        var text = new TextBlock
        {
          Text = "Click currency buttons to add tip",
          FontSize = 16,
          FontStyle = FontStyles.Italic,
          Foreground = new SolidColorBrush(Colors.Gray),
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(0, 20, 0, 20)
        };
        placeholder.Child = text;
        AddTipRowsPanel.Items.Add(placeholder);
      }
      else
      {
        int fontSize = addedDenoms.Count <= 4 ? 22 : addedDenoms.Count <= 6 ? 18 : addedDenoms.Count <= 8 ? 14 : 12;
        int countFontSize = addedDenoms.Count <= 4 ? 28 : addedDenoms.Count <= 6 ? 24 : addedDenoms.Count <= 8 ? 18 : 14;
        foreach (int denom in addedDenoms.OrderByDescending(d => d))
        {
          var row = new Border
          {
            BorderBrush = new SolidColorBrush(Color.FromRgb(100, 100, 100)),
            BorderThickness = new Thickness(2),
            Background = new SolidColorBrush(Color.FromRgb(245, 255, 245)),
            CornerRadius = new CornerRadius(5),
            Margin = new Thickness(0, 1, 0, 1)
          };
          var grid = new Grid();
          grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
          grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
          grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
          var denominationText = new TextBlock
          {
            Text = FormatDenom(denom),
            FontSize = fontSize,
            FontWeight = FontWeights.SemiBold,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(4)
          };
          Grid.SetColumn(denominationText, 0);
          var countText = new TextBlock
          {
            Text = "×" + _addTipCounts[denom].ToString(),
            FontSize = countFontSize,
            FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromRgb(0, 100, 0)),
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(4)
          };
          Grid.SetColumn(countText, 1);
          var valueText = new TextBlock
          {
            Text = (_addTipCounts[denom] * denom / 100.0).ToString("C", culture),
            FontSize = fontSize,
            FontWeight = FontWeights.SemiBold,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(4)
          };
          Grid.SetColumn(valueText, 2);
          grid.Children.Add(denominationText);
          grid.Children.Add(countText);
          grid.Children.Add(valueText);
          row.Child = grid;
          AddTipRowsPanel.Items.Add(row);
        }
        if (addedDenoms.Count < 4)
        {
          for (int i = 0; i < 4 - addedDenoms.Count; i++)
          {
            AddTipRowsPanel.Items.Add(new Border { Background = Brushes.Transparent, Margin = new Thickness(0, 1, 0, 1) });
          }
        }
      }

      UpdateAddTipButtonCounts();
      double total = _addTipCounts.Sum(kv => kv.Key * kv.Value) / 100.0;
      AddTipTotalText.Text = total.ToString("C", culture);
    }

    private void UpdateAddTipButtonCounts()
    {
      AddTip5cCount.Text = "×" + _addTipCounts.GetValueOrDefault(5, 0);
      AddTip10cCount.Text = "×" + _addTipCounts.GetValueOrDefault(10, 0);
      AddTip20cCount.Text = "×" + _addTipCounts.GetValueOrDefault(20, 0);
      AddTip50cCount.Text = "×" + _addTipCounts.GetValueOrDefault(50, 0);
      AddTip1Count.Text = "×" + _addTipCounts.GetValueOrDefault(100, 0);
      AddTip2Count.Text = "×" + _addTipCounts.GetValueOrDefault(200, 0);
      AddTip5Count.Text = "×" + _addTipCounts.GetValueOrDefault(500, 0);
      AddTip10Count.Text = "×" + _addTipCounts.GetValueOrDefault(1000, 0);
      AddTip20Count.Text = "×" + _addTipCounts.GetValueOrDefault(2000, 0);
      AddTip50Count.Text = "×" + _addTipCounts.GetValueOrDefault(5000, 0);
      AddTip100Count.Text = "×" + _addTipCounts.GetValueOrDefault(10000, 0);
    }

    private void AddTipConfirm_Click(object sender, RoutedEventArgs e)
    {
      var totalCents = _addTipCounts.Sum(kv => kv.Key * kv.Value);
      if (totalCents == 0)
      {
        MessageBox.Show("Please add at least one denomination for the tip.", "No Tip Amount");
        return;
      }

      var culture = CultureInfo.GetCultureInfo("en-AU");
      var batchId = Guid.NewGuid().ToString("N");

      Database.InTransaction(tx =>
      {
        foreach (var kv in _addTipCounts.Where(kv => kv.Value > 0))
        {
          Database.Exec(
            "INSERT INTO tips(denom_cents, qty, notes, batch_id) VALUES ($d, $q, $n, $b)",
            tx, ("$d", kv.Key), ("$q", kv.Value), ("$n", "Cash tip received"), ("$b", batchId)
          );
          Database.Exec(
            "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, notes, batch_id) VALUES ($d, $q, 'TIP', $n, $b)",
            tx, ("$d", kv.Key), ("$q", kv.Value), ("$n", "Cash tip received"), ("$b", batchId)
          );
        }
        var tipDesc = string.Join(", ", _addTipCounts.Where(kv => kv.Value > 0).Select(kv => $"{kv.Value}×{FormatDenom(kv.Key)}"));
        Database.Exec(
          "INSERT INTO activity_log(activity_key, activity_type, activity_kind, method, staff, amount_cents, notes, batch_id) " +
          "VALUES ($key, 'TIP', 'TIP', 'MANUAL', 'STAFF', $amount, $notes, $batch)",
          tx,
          ("$key", Guid.NewGuid().ToString("N")),
          ("$amount", totalCents),
          ("$notes", $"Cash tip: {tipDesc}"),
          ("$batch", batchId)
        );
      });

      _addTipCounts = CashDenoms.ToDictionary(d => d, d => 0);
      _addTipHistory.Clear();
      ShowMainCashboxView();
      RefreshCashbox();
    }

    private void BackToMain_Click(object sender, RoutedEventArgs e)
    {
      if (FloatInputView.Visibility == Visibility.Visible || ExchangeView.Visibility == Visibility.Visible || AddTipView.Visibility == Visibility.Visible)
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
      ExchangeView.Visibility = Visibility.Collapsed;
      AddTipView.Visibility = Visibility.Collapsed;
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
      FloatInputRowsPanel.Items.Clear();

      var culture = CultureInfo.GetCultureInfo("en-AU");

      // Only show denominations that have been added (count > 0)
      var addedDenoms = CashDenoms.Where(d => _floatCounts.ContainsKey(d) && _floatCounts[d] > 0).ToList();

      if (addedDenoms.Count == 0)
      {
        // Show placeholder when no cash has been added
        var placeholder = new Border
        {
          BorderBrush = new SolidColorBrush(Colors.Gray),
          BorderThickness = new Thickness(1, 0, 1, 1),
          Background = new SolidColorBrush(Color.FromRgb(250, 250, 250))
        };

        var text = new TextBlock
        {
          Text = "Click currency buttons to add float",
          FontSize = 16,
          FontStyle = FontStyles.Italic,
          Foreground = new SolidColorBrush(Colors.Gray),
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(0, 20, 0, 20)
        };

        placeholder.Child = text;
        FloatInputRowsPanel.Items.Add(placeholder);
      }
      else
      {
        // Scale font down when there are many items
        int fontSize;
        int countFontSize;
        if (addedDenoms.Count <= 4)
        {
          fontSize = 22;
          countFontSize = 28;
        }
        else if (addedDenoms.Count <= 6)
        {
          fontSize = 18;
          countFontSize = 24;
        }
        else if (addedDenoms.Count <= 8)
        {
          fontSize = 14;
          countFontSize = 18;
        }
        else
        {
          fontSize = 12;
          countFontSize = 14;
        }

        // Add the actual denomination rows (ordered by descending value)
        foreach (int denom in addedDenoms.OrderByDescending(d => d))
        {
          // Consistent boxed look for all rows
          var row = new Border
          {
            BorderBrush = new SolidColorBrush(Color.FromRgb(100, 100, 100)),
            BorderThickness = new Thickness(2),
            Background = new SolidColorBrush(Color.FromRgb(245, 255, 245)),
            CornerRadius = new CornerRadius(5),
            Margin = new Thickness(0, 1, 0, 1)
          };

          var grid = new Grid();
          grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
          grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
          grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });

          var denominationText = new TextBlock
          {
            Text = FormatDenom(denom),
            FontSize = fontSize,
            FontWeight = FontWeights.SemiBold,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(4)
          };
          Grid.SetColumn(denominationText, 0);

          var countText = new TextBlock
          {
            Text = "×" + _floatCounts[denom].ToString(),
            FontSize = countFontSize,
            FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromRgb(0, 100, 0)),
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(4)
          };
          Grid.SetColumn(countText, 1);

          var valueText = new TextBlock
          {
            Text = (_floatCounts[denom] * denom / 100.0).ToString("C", culture),
            FontSize = fontSize,
            FontWeight = FontWeights.SemiBold,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(4)
          };
          Grid.SetColumn(valueText, 2);

          grid.Children.Add(denominationText);
          grid.Children.Add(countText);
          grid.Children.Add(valueText);
          row.Child = grid;
          FloatInputRowsPanel.Items.Add(row);
        }

        // When 1-3 items, add invisible spacers so they're sized as if there were 4
        if (addedDenoms.Count < 4)
        {
          int spacersNeeded = 4 - addedDenoms.Count;
          for (int i = 0; i < spacersNeeded; i++)
          {
            var spacer = new Border
            {
              Background = Brushes.Transparent,
              Margin = new Thickness(0, 1, 0, 1)
            };
            FloatInputRowsPanel.Items.Add(spacer);
          }
        }
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
      Float5cCount.Text = "×" + _floatCounts[5].ToString();
      Float10cCount.Text = "×" + _floatCounts[10].ToString();
      Float20cCount.Text = "×" + _floatCounts[20].ToString();
      Float50cCount.Text = "×" + _floatCounts[50].ToString();
      Float1Count.Text = "×" + _floatCounts[100].ToString();
      Float2Count.Text = "×" + _floatCounts[200].ToString();
      Float5Count.Text = "×" + _floatCounts[500].ToString();
      Float10Count.Text = "×" + _floatCounts[1000].ToString();
      Float20Count.Text = "×" + _floatCounts[2000].ToString();
      Float50Count.Text = "×" + _floatCounts[5000].ToString();
      Float100Count.Text = "×" + _floatCounts[10000].ToString();
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

    private void FloatMultiplier1x_Click(object sender, RoutedEventArgs e)
    {
      _currentMultiplier = 1;
      UpdateFloatMultiplierDisplay();
    }

    private void FloatMultiplier5x_Click(object sender, RoutedEventArgs e)
    {
      _currentMultiplier = 5;
      UpdateFloatMultiplierDisplay();
    }

    private void FloatMultiplier20x_Click(object sender, RoutedEventArgs e)
    {
      _currentMultiplier = 20;
      UpdateFloatMultiplierDisplay();
    }

    private void UpdateFloatMultiplierDisplay()
    {
      // Reset all to default (light blue)
      Float1xButton.Background = Brushes.LightBlue;
      Float5xButton.Background = Brushes.LightBlue;
      Float20xButton.Background = Brushes.LightBlue;

      // Highlight current multiplier (light green)
      switch (_currentMultiplier)
      {
        case 1:
          Float1xButton.Background = Brushes.LightGreen;
          break;
        case 5:
          Float5xButton.Background = Brushes.LightGreen;
          break;
        case 20:
          Float20xButton.Background = Brushes.LightGreen;
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

    private void UpdateTotalSalesDisplay()
    {
      try
      {
        // Calculate total money made from all food sales
        // Method 1: Direct food sales (activity_kind = 'FOOD')
        // Method 2: Food sales during cashout - parse from notes field
        // Notes format is either "Food ($X.XX): items" or "Food: $X.XX"
        var totalCents = Database.ScalarLong(@"
SELECT COALESCE(SUM(food_amount), 0) FROM (
  -- Direct food sales
  SELECT amount_cents as food_amount
  FROM activity_log 
  WHERE activity_kind = 'FOOD'
  
  UNION ALL
  
  -- Food during cashout with format 'Food ($X.XX): items'
  SELECT CAST(
    REPLACE(
      SUBSTR(notes, INSTR(notes, 'Food ($') + 7, 
        INSTR(SUBSTR(notes, INSTR(notes, 'Food ($') + 7), ')') - 1
      ), ',', ''
    ) AS REAL
  ) * 100 as food_amount
  FROM activity_log 
  WHERE activity_kind = 'TX' 
    AND (activity_type = 'CASHOUT' OR activity_type = 'CASHOUT_SALE')
    AND notes LIKE '%Food ($%'
    
  UNION ALL
  
  -- Food during cashout with format 'Food: $X.XX' (legacy/fallback)
  SELECT CAST(
    REPLACE(
      SUBSTR(notes, INSTR(notes, 'Food: $') + 7, 
        CASE 
          WHEN INSTR(SUBSTR(notes, INSTR(notes, 'Food: $') + 7), ' |') > 0 
          THEN INSTR(SUBSTR(notes, INSTR(notes, 'Food: $') + 7), ' |') - 1
          ELSE LENGTH(SUBSTR(notes, INSTR(notes, 'Food: $') + 7))
        END
      ), ',', ''
    ) AS REAL
  ) * 100 as food_amount
  FROM activity_log 
  WHERE activity_kind = 'TX' 
    AND (activity_type = 'CASHOUT' OR activity_type = 'CASHOUT_SALE')
    AND notes LIKE '%Food: $%'
    AND notes NOT LIKE '%Food ($%'
)");

        var totalAmount = totalCents / 100.0;
        var AU = CultureInfo.GetCultureInfo("en-AU");
        TotalSalesAmount.Text = totalAmount.ToString("C", AU);
      }
      catch (Exception ex)
      {
        TotalSalesAmount.Text = "$0.00";
        System.Diagnostics.Debug.WriteLine($"Error updating total sales: {ex.Message}");
      }
    }

    // ===== EXCHANGE FUNCTIONALITY =====
    private Dictionary<int, int> _exchangeGiveOut = new();
    private Dictionary<int, int> _exchangeReceive = new();
    private bool _exchangeGiveOutMode = true; // true = adding to Give Out, false = adding to Receive

    private void Exchange_Click(object sender, RoutedEventArgs e)
    {
      ShowExchangeView();
    }

    private void ShowExchangeView()
    {
      MainCashboxView.Visibility = Visibility.Collapsed;
      FloatInputView.Visibility = Visibility.Collapsed;
      ExchangeView.Visibility = Visibility.Visible;
      
      // Initialize exchange
      _exchangeGiveOut = CashDenoms.ToDictionary(d => d, d => 0);
      _exchangeReceive = CashDenoms.ToDictionary(d => d, d => 0);
      _exchangeGiveOutMode = true;
      
      RefreshExchangePanels();
      UpdateExchangeModeButtons();
    }

    private void ExchangeGiveOutMode_Click(object sender, RoutedEventArgs e)
    {
      _exchangeGiveOutMode = true;
      UpdateExchangeModeButtons();
    }

    private void ExchangeReceiveMode_Click(object sender, RoutedEventArgs e)
    {
      _exchangeGiveOutMode = false;
      UpdateExchangeModeButtons();
    }

    private void UpdateExchangeModeButtons()
    {
      if (_exchangeGiveOutMode)
      {
        ExchangeGiveOutModeBtn.Background = new SolidColorBrush(Color.FromRgb(255, 224, 224));
        ExchangeGiveOutModeBtn.BorderBrush = new SolidColorBrush(Colors.Crimson);
        ExchangeGiveOutModeBtn.BorderThickness = new Thickness(3);
        
        ExchangeReceiveModeBtn.Background = new SolidColorBrush(Colors.LightGray);
        ExchangeReceiveModeBtn.BorderBrush = new SolidColorBrush(Colors.Gray);
        ExchangeReceiveModeBtn.BorderThickness = new Thickness(2);
      }
      else
      {
        ExchangeGiveOutModeBtn.Background = new SolidColorBrush(Colors.LightGray);
        ExchangeGiveOutModeBtn.BorderBrush = new SolidColorBrush(Colors.Gray);
        ExchangeGiveOutModeBtn.BorderThickness = new Thickness(2);
        
        ExchangeReceiveModeBtn.Background = new SolidColorBrush(Color.FromRgb(224, 255, 224));
        ExchangeReceiveModeBtn.BorderBrush = new SolidColorBrush(Colors.Green);
        ExchangeReceiveModeBtn.BorderThickness = new Thickness(3);
      }
    }

    private void ExchangeDenom_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button button && int.TryParse(button.Tag?.ToString(), out int denom))
      {
        if (_exchangeGiveOutMode)
        {
          _exchangeGiveOut[denom]++;
        }
        else
        {
          _exchangeReceive[denom]++;
        }
        RefreshExchangePanels();
      }
    }

    private void ExchangeClear_Click(object sender, RoutedEventArgs e)
    {
      _exchangeGiveOut = CashDenoms.ToDictionary(d => d, d => 0);
      _exchangeReceive = CashDenoms.ToDictionary(d => d, d => 0);
      RefreshExchangePanels();
    }

    private void RefreshExchangePanels()
    {
      var culture = CultureInfo.GetCultureInfo("en-AU");
      
      // Refresh Give Out panel
      RefreshExchangePanel(ExchangeGiveOutPanel, _exchangeGiveOut, Colors.Crimson);
      
      // Refresh Receive panel
      RefreshExchangePanel(ExchangeReceivePanel, _exchangeReceive, Colors.Green);
      
      // Update totals
      int giveOutTotal = _exchangeGiveOut.Sum(kv => kv.Key * kv.Value);
      int receiveTotal = _exchangeReceive.Sum(kv => kv.Key * kv.Value);
      int balance = receiveTotal - giveOutTotal;
      
      ExchangeGiveOutTotal.Text = (giveOutTotal / 100.0).ToString("C", culture);
      ExchangeReceiveTotal.Text = (receiveTotal / 100.0).ToString("C", culture);
      ExchangeBalanceText.Text = (balance / 100.0).ToString("C", culture);
      
      // Update balance color and confirm button
      if (balance == 0 && giveOutTotal > 0)
      {
        ExchangeBalanceText.Foreground = new SolidColorBrush(Colors.Green);
        ExchangeBalanceBorder.Background = new SolidColorBrush(Color.FromRgb(224, 255, 224));
        ExchangeBalanceBorder.BorderBrush = new SolidColorBrush(Colors.Green);
        ExchangeConfirmButton.IsEnabled = true;
      }
      else
      {
        ExchangeBalanceText.Foreground = balance < 0 ? new SolidColorBrush(Colors.Crimson) : new SolidColorBrush(Colors.Orange);
        ExchangeBalanceBorder.Background = new SolidColorBrush(Colors.LightYellow);
        ExchangeBalanceBorder.BorderBrush = new SolidColorBrush(Colors.Orange);
        ExchangeConfirmButton.IsEnabled = false;
      }
    }

    private void RefreshExchangePanel(ItemsControl panel, Dictionary<int, int> counts, Color borderColor)
    {
      panel.Items.Clear();
      var culture = CultureInfo.GetCultureInfo("en-AU");
      
      var addedDenoms = counts.Where(kv => kv.Value > 0).OrderByDescending(kv => kv.Key).ToList();
      
      if (addedDenoms.Count == 0)
      {
        var placeholder = new Border
        {
          BorderBrush = new SolidColorBrush(borderColor),
          BorderThickness = new Thickness(2, 0, 2, 2),
          Background = new SolidColorBrush(Color.FromRgb(250, 250, 250))
        };

        var text = new TextBlock
        {
          Text = "Click denominations",
          FontSize = 12,
          FontStyle = FontStyles.Italic,
          Foreground = new SolidColorBrush(Colors.Gray),
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(0, 10, 0, 10)
        };

        placeholder.Child = text;
        panel.Items.Add(placeholder);
        return;
      }

      int fontSize = addedDenoms.Count <= 3 ? 14 : addedDenoms.Count <= 5 ? 12 : 10;
      int countFontSize = addedDenoms.Count <= 3 ? 16 : addedDenoms.Count <= 5 ? 14 : 12;

      foreach (var kv in addedDenoms)
      {
        var row = new Border
        {
          BorderBrush = new SolidColorBrush(borderColor),
          BorderThickness = new Thickness(2, 0, 2, 1),
          Background = new SolidColorBrush(Color.FromRgb(255, 255, 255)),
          Margin = new Thickness(0, 0, 0, 1)
        };

        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });

        var denomText = new TextBlock
        {
          Text = FormatDenom(kv.Key),
          FontSize = fontSize,
          FontWeight = FontWeights.SemiBold,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(2)
        };
        Grid.SetColumn(denomText, 0);

        var countText = new TextBlock
        {
          Text = "×" + kv.Value.ToString(),
          FontSize = countFontSize,
          FontWeight = FontWeights.Bold,
          Foreground = new SolidColorBrush(borderColor),
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(2)
        };
        Grid.SetColumn(countText, 1);

        var valueText = new TextBlock
        {
          Text = (kv.Value * kv.Key / 100.0).ToString("C", culture),
          FontSize = fontSize,
          FontWeight = FontWeights.SemiBold,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(2)
        };
        Grid.SetColumn(valueText, 2);

        grid.Children.Add(denomText);
        grid.Children.Add(countText);
        grid.Children.Add(valueText);
        row.Child = grid;
        panel.Items.Add(row);
      }

      // Add spacers if less than 3 items
      if (addedDenoms.Count < 3)
      {
        int spacersNeeded = 3 - addedDenoms.Count;
        for (int i = 0; i < spacersNeeded; i++)
        {
          var spacer = new Border
          {
            Background = Brushes.Transparent,
            Margin = new Thickness(0, 0, 0, 1)
          };
          panel.Items.Add(spacer);
        }
      }
    }

    private void ExchangeCancel_Click(object sender, RoutedEventArgs e)
    {
      ShowMainCashboxView();
    }

    private void ExchangeConfirm_Click(object sender, RoutedEventArgs e)
    {
      var culture = CultureInfo.GetCultureInfo("en-AU");
      int giveOutTotal = _exchangeGiveOut.Sum(kv => kv.Key * kv.Value);
      int receiveTotal = _exchangeReceive.Sum(kv => kv.Key * kv.Value);
      
      if (giveOutTotal != receiveTotal || giveOutTotal == 0)
      {
        MessageBox.Show("Exchange amounts must be equal and greater than zero.", "Invalid Exchange");
        return;
      }

      Database.InTransaction(tx =>
      {
        var batchId = Guid.NewGuid().ToString("N");
        
        // Remove give out amounts from cashbox
        foreach (var kv in _exchangeGiveOut)
        {
          if (kv.Value <= 0) continue;
          Database.Exec(
            "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id, batch_id) " +
            "VALUES ($d, $q, 'EXCHANGE', NULL, NULL, $batch)",
            tx, ("$d", kv.Key), ("$q", -kv.Value), ("$batch", batchId)
          );
        }
        
        // Add receive amounts to cashbox
        foreach (var kv in _exchangeReceive)
        {
          if (kv.Value <= 0) continue;
          Database.Exec(
            "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id, batch_id) " +
            "VALUES ($d, $q, 'EXCHANGE', NULL, NULL, $batch)",
            tx, ("$d", kv.Key), ("$q", kv.Value), ("$batch", batchId)
          );
        }
        
        // Build notes describing the exchange
        var giveOutDesc = string.Join(", ", _exchangeGiveOut.Where(kv => kv.Value > 0).Select(kv => $"{kv.Value}×{FormatDenom(kv.Key)}"));
        var receiveDesc = string.Join(", ", _exchangeReceive.Where(kv => kv.Value > 0).Select(kv => $"{kv.Value}×{FormatDenom(kv.Key)}"));
        
        Database.Exec(
          "INSERT INTO activity_log(activity_key, activity_type, activity_kind, method, staff, player_id, tx_id, batch_id, amount_cents, notes) " +
          "VALUES ($key, 'EXCHANGE', 'EXCHANGE', 'MANUAL', 'STAFF', NULL, NULL, $batch, $amount, $notes)",
          tx, 
          ("$key", Guid.NewGuid().ToString("N")),
          ("$batch", batchId),
          ("$amount", giveOutTotal),
          ("$notes", $"Exchange: Gave [{giveOutDesc}] for [{receiveDesc}]")
        );
      });

      // Reset and return to main view
      _exchangeGiveOut = CashDenoms.ToDictionary(d => d, d => 0);
      _exchangeReceive = CashDenoms.ToDictionary(d => d, d => 0);
      ShowMainCashboxView();
      RefreshCashbox();
    }
  }
}
