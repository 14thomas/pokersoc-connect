using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;

namespace pokersoc_connect
{
  public partial class FloatWindow : Window
  {
    public Dictionary<int,int> Counts { get; } = new()
    {
      { 5,0 }, {10,0}, {20,0}, {50,0},
      {100,0}, {200,0},
      {500,0}, {1000,0}, {2000,0}, {5000,0}, {10000,0}
    };

    private int _multiplier = 1;
    private readonly Stack<int> _history = new();

    public FloatWindow(Dictionary<int,int>? preset = null)
    {
      InitializeComponent();
      if (preset != null)
        foreach (var kv in preset) if (Counts.ContainsKey(kv.Key)) Counts[kv.Key] = kv.Value;

      Loaded += (s,e) => { UpdateAllButtonCounts(); UpdateTotal(); RefreshCashInput(); UpdateMultiplierDisplay(); };
    }

    private void UpdateAllButtonCounts()
    {
      B5Count.Text = $"×{Counts[5]}";
      B10Count.Text = $"×{Counts[10]}";
      B20Count.Text = $"×{Counts[20]}";
      B50Count.Text = $"×{Counts[50]}";
      B100Count.Text = $"×{Counts[100]}";
      B200Count.Text = $"×{Counts[200]}";
      B500Count.Text = $"×{Counts[500]}";
      B1000Count.Text = $"×{Counts[1000]}";
      B2000Count.Text = $"×{Counts[2000]}";
      B5000Count.Text = $"×{Counts[5000]}";
      B10000Count.Text = $"×{Counts[10000]}";
    }

    private void UpdateTotal()
    {
      var cents = Counts.Sum(kv => kv.Key * kv.Value);
      TotalText.Text = (cents / 100.0).ToString("C", CultureInfo.GetCultureInfo("en-AU"));
    }

    private void RefreshCashInput()
    {
      CashInputRowsPanel.Items.Clear();
      var culture = CultureInfo.GetCultureInfo("en-AU");

      // Only show denominations that have been added (count > 0)
      var addedDenoms = Counts.Where(kv => kv.Value > 0).OrderByDescending(kv => kv.Key).ToList();

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
          Text = "Click currency buttons to add cash",
          FontSize = 16,
          FontStyle = FontStyles.Italic,
          Foreground = new SolidColorBrush(Colors.Gray),
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(0, 20, 0, 20)
        };

        placeholder.Child = text;
        CashInputRowsPanel.Items.Add(placeholder);
        return;
      }

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

      // Add the actual denomination rows
      foreach (var kv in addedDenoms)
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
          Text = FormatDenom(kv.Key),
          FontSize = fontSize,
          FontWeight = FontWeights.SemiBold,
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center,
          Margin = new Thickness(4)
        };
        Grid.SetColumn(denominationText, 0);

        var countText = new TextBlock
        {
          Text = "×" + kv.Value.ToString(),
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
          Text = (kv.Value * kv.Key / 100.0).ToString("C", culture),
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
        CashInputRowsPanel.Items.Add(row);
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
          CashInputRowsPanel.Items.Add(spacer);
        }
      }
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

    private void Denom_Click(object sender, RoutedEventArgs e)
    {
      if (sender is not Button b || b.Tag is null || !int.TryParse(b.Tag.ToString(), out var cents)) return;
      var add = _multiplier <= 0 ? 1 : _multiplier;
      Counts[cents] = Counts.TryGetValue(cents, out var c) ? c + add : add;
      // Add to history for undo functionality
      for (int i = 0; i < add; i++)
      {
        _history.Push(cents);
      }
      UpdateAllButtonCounts();
      UpdateTotal();
      RefreshCashInput();
    }

    private void Multiplier_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button b && b.Tag is string tag && int.TryParse(tag, out var mult))
      {
        _multiplier = mult;
        UpdateMultiplierDisplay();
      }
    }

    private void UpdateMultiplierDisplay()
    {
      X1.Background = _multiplier == 1 ? Brushes.LightGreen : Brushes.LightBlue;
      X5.Background = _multiplier == 5 ? Brushes.LightGreen : Brushes.LightBlue;
      X10.Background = _multiplier == 10 ? Brushes.LightGreen : Brushes.LightBlue;
    }

    private void Clear_Click(object sender, RoutedEventArgs e)
    {
      foreach (var k in Counts.Keys.ToList()) Counts[k] = 0;
      _history.Clear();
      UpdateAllButtonCounts();
      UpdateTotal();
      RefreshCashInput();
    }

    private void Undo_Click(object sender, RoutedEventArgs e)
    {
      if (_history.Count > 0)
      {
        var lastDenom = _history.Pop();
        if (Counts.ContainsKey(lastDenom) && Counts[lastDenom] > 0)
        {
          Counts[lastDenom]--;
          UpdateAllButtonCounts();
          UpdateTotal();
          RefreshCashInput();
        }
      }
    }

    private void Save_Click(object sender, RoutedEventArgs e) => DialogResult = true;

    private void Cancel_Click(object sender, RoutedEventArgs e) => DialogResult = false;
  }
}
