using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace pokersoc_connect.Views
{
  public partial class LostChipsView : UserControl
  {
    // Poker chip denominations only
    private static readonly int[] PokerChipDenoms = { 5, 25, 100, 500, 2500, 10000 };
    private readonly Dictionary<int, TextBlock> _chipCounts = new Dictionary<int, TextBlock>();
    private readonly Dictionary<int, int> _chipValues = new Dictionary<int, int>();
    private int _currentMultiplier = 1;
    private readonly List<(int denom, int count)> _undoStack = new List<(int, int)>();

    public event EventHandler? CloseRequested;

    public LostChipsView()
    {
      InitializeComponent();
      InitializeChipCounts();
      UpdateTotal();
      RefreshSummaryTable();
      UpdateMultiplierDisplay();
    }

    private void InitializeChipCounts()
    {
      _chipCounts[5] = Chip5c;
      _chipCounts[25] = Chip25c;
      _chipCounts[100] = Chip1;
      _chipCounts[500] = Chip5;
      _chipCounts[2500] = Chip25;
      _chipCounts[10000] = Chip100;

      foreach (var denom in PokerChipDenoms)
      {
        _chipValues[denom] = 0;
      }
    }

    private void AddChip(int denom, int count = 1)
    {
      _undoStack.Add((denom, _chipValues[denom]));
      
      _chipValues[denom] += count * _currentMultiplier;
      _chipCounts[denom].Text = $"×{_chipValues[denom]}";
      UpdateTotal();
      RefreshSummaryTable();
    }

    private void UpdateTotal()
    {
      double totalCents = _chipValues.Sum(kvp => kvp.Value * kvp.Key);
      var au = CultureInfo.GetCultureInfo("en-AU");
      TotalTipsText.Text = (totalCents / 100.0).ToString("C", au);
    }

    private void RefreshSummaryTable()
    {
      ChipSummaryPanel.Children.Clear();
      var au = CultureInfo.GetCultureInfo("en-AU");

      foreach (var denom in PokerChipDenoms)
      {
        if (_chipValues[denom] > 0)
        {
          var row = new Border
          {
            BorderBrush = new SolidColorBrush(Colors.Gray),
            BorderThickness = new Thickness(1, 0, 1, 1),
            Height = 36
          };

          var grid = new Grid();
          grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(100) });
          grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
          grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(100) });

          var chipText = new TextBlock
          {
            Text = GetDenomLabel(denom) + " " + GetChipColor(denom),
            FontSize = 13,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(5)
          };
          Grid.SetColumn(chipText, 0);

          var countText = new TextBlock
          {
            Text = _chipValues[denom].ToString(),
            FontSize = 13,
            FontWeight = FontWeights.Bold,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(5)
          };
          Grid.SetColumn(countText, 1);

          var valueText = new TextBlock
          {
            Text = ((_chipValues[denom] * denom) / 100.0).ToString("C", au),
            FontSize = 13,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(5)
          };
          Grid.SetColumn(valueText, 2);

          grid.Children.Add(chipText);
          grid.Children.Add(countText);
          grid.Children.Add(valueText);
          row.Child = grid;
          ChipSummaryPanel.Children.Add(row);
        }
      }

      // Show placeholder if no chips
      if (!_chipValues.Any(kvp => kvp.Value > 0))
      {
        var placeholder = new Border
        {
          BorderBrush = new SolidColorBrush(Colors.Gray),
          BorderThickness = new Thickness(1, 0, 1, 1),
          Height = 50,
          Background = new SolidColorBrush(Color.FromRgb(250, 250, 250))
        };

        var text = new TextBlock
        {
          Text = "Click chips on the right to add them",
          FontSize = 13,
          FontStyle = FontStyles.Italic,
          Foreground = new SolidColorBrush(Colors.Gray),
          VerticalAlignment = VerticalAlignment.Center,
          HorizontalAlignment = HorizontalAlignment.Center
        };

        placeholder.Child = text;
        ChipSummaryPanel.Children.Add(placeholder);
      }
    }

    private void UpdateMultiplierDisplay()
    {
      // Reset all to default (light blue)
      Toggle1x.Background = Brushes.LightBlue;
      Toggle5x.Background = Brushes.LightBlue;
      Toggle20x.Background = Brushes.LightBlue;

      // Highlight current multiplier (light green)
      switch (_currentMultiplier)
      {
        case 1:
          Toggle1x.Background = Brushes.LightGreen;
          break;
        case 5:
          Toggle5x.Background = Brushes.LightGreen;
          break;
        case 20:
          Toggle20x.Background = Brushes.LightGreen;
          break;
      }
    }

    // Add chip handlers
    private void AddChip5c_Click(object sender, RoutedEventArgs e) => AddChip(5);
    private void AddChip25c_Click(object sender, RoutedEventArgs e) => AddChip(25);
    private void AddChip1_Click(object sender, RoutedEventArgs e) => AddChip(100);
    private void AddChip5_Click(object sender, RoutedEventArgs e) => AddChip(500);
    private void AddChip25_Click(object sender, RoutedEventArgs e) => AddChip(2500);
    private void AddChip100_Click(object sender, RoutedEventArgs e) => AddChip(10000);

    // Toggle button handlers
    private void Toggle1x_Click(object sender, RoutedEventArgs e)
    {
      _currentMultiplier = 1;
      UpdateMultiplierDisplay();
    }

    private void Toggle5x_Click(object sender, RoutedEventArgs e)
    {
      _currentMultiplier = 5;
      UpdateMultiplierDisplay();
    }

    private void Toggle20x_Click(object sender, RoutedEventArgs e)
    {
      _currentMultiplier = 20;
      UpdateMultiplierDisplay();
    }

    private void RecordTips_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        var tipsToRecord = _chipValues.Where(kvp => kvp.Value > 0).ToList();

        if (!tipsToRecord.Any())
        {
          ConfirmationText.Text = "Please click on at least one chip to record as tips.";
          ConfirmationPanel.Visibility = Visibility.Visible;
          return;
        }

        var totalValue = tipsToRecord.Sum(t => t.Value * t.Key);
        var totalChips = tipsToRecord.Sum(t => t.Value);
        var au = CultureInfo.GetCultureInfo("en-AU");

        var chipBreakdown = string.Join("\n", tipsToRecord.Select(t => 
          $"  • {GetDenomLabel(t.Key)} ({GetChipColor(t.Key)}): {t.Value}"));

        ConfirmationText.Text = $"Record {totalChips} lost chip{(totalChips > 1 ? "s" : "")} as tips?\n\n{chipBreakdown}\n\nTotal value: {(totalValue / 100.0).ToString("C", au)}";
        ConfirmationPanel.Visibility = Visibility.Visible;
      }
      catch (Exception ex)
      {
        ConfirmationText.Text = $"Error: {ex.Message}";
        ConfirmationPanel.Visibility = Visibility.Visible;
      }
    }

    private void ConfirmRecordTips_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        var tipsToRecord = _chipValues.Where(kvp => kvp.Value > 0).ToList();
        if (!tipsToRecord.Any())
        {
          ConfirmationPanel.Visibility = Visibility.Collapsed;
          return;
        }

        var totalValue = tipsToRecord.Sum(t => t.Value * t.Key);
        
        string batchId = Guid.NewGuid().ToString("N");
        
        Database.InTransaction(tx =>
        {
          foreach (var (denom, qty) in tipsToRecord)
          {
            Database.Exec(
              "INSERT INTO tips(denom_cents, qty, notes) VALUES ($d, $q, $n)",
              tx, ("$d", denom), ("$q", qty), ("$n", "Lost chip recorded as tip")
            );

            Database.Exec(
              "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, notes, batch_id) VALUES ($d, $q, 'LOST_CHIP', $n, $b)",
              tx, ("$d", denom), ("$q", qty), ("$n", "Lost chip recorded as tip"), ("$b", batchId)
            );
          }
          
          var chipBreakdown = string.Join(", ", tipsToRecord.Select(t => 
          {
            var denomLabel = GetDenomLabel(t.Key);
            return t.Value > 1 ? $"{denomLabel} x{t.Value}" : denomLabel;
          }));
          
          Database.Exec(
            "INSERT INTO activity_log(activity_key, activity_type, activity_kind, amount_cents, notes, batch_id) " +
            "VALUES ($key, $type, $kind, $amount, $notes, $batch)",
            tx, 
            ("$key", $"lost_chips_{batchId}"),
            ("$type", "LOST_CHIPS"),
            ("$kind", "LOST_CHIP"),
            ("$amount", totalValue),
            ("$notes", $"Lost chips recorded: {chipBreakdown}"),
            ("$batch", batchId)
          );
        });

        ConfirmationPanel.Visibility = Visibility.Collapsed;
        ClearAllChips();
      }
      catch (Exception ex)
      {
        ConfirmationText.Text = $"Error recording lost chips: {ex.Message}";
        ConfirmationPanel.Visibility = Visibility.Visible;
      }
    }

    private void CancelConfirmation_Click(object sender, RoutedEventArgs e)
    {
      ConfirmationPanel.Visibility = Visibility.Collapsed;
    }

    private void UndoLast_Click(object sender, RoutedEventArgs e)
    {
      if (_undoStack.Count > 0)
      {
        var (denom, previousCount) = _undoStack[_undoStack.Count - 1];
        _undoStack.RemoveAt(_undoStack.Count - 1);
        
        _chipValues[denom] = previousCount;
        _chipCounts[denom].Text = $"×{_chipValues[denom]}";
        UpdateTotal();
        RefreshSummaryTable();
      }
    }

    private void ClearAll_Click(object sender, RoutedEventArgs e)
    {
      // Save current state for undo
      foreach (var denom in PokerChipDenoms)
      {
        if (_chipValues[denom] > 0)
        {
          _undoStack.Add((denom, _chipValues[denom]));
        }
      }
      
      ClearAllChips();
    }

    private void ClearAllChips()
    {
      foreach (var denom in PokerChipDenoms)
      {
        _chipValues[denom] = 0;
        _chipCounts[denom].Text = "×0";
      }
      UpdateTotal();
      RefreshSummaryTable();
    }

    private void Close_Click(object sender, RoutedEventArgs e)
    {
      CloseRequested?.Invoke(this, EventArgs.Empty);
    }
    
    private static string GetDenomLabel(int cents) => cents switch
    {
      5 => "5c",
      25 => "25c",
      100 => "$1",
      500 => "$5",
      2500 => "$25",
      10000 => "$100",
      _ => $"{cents}c"
    };

    private static string GetChipColor(int cents) => cents switch
    {
      5 => "White",
      25 => "Red",
      100 => "Blue",
      500 => "Black",
      2500 => "White Plaque",
      10000 => "Red Plaque",
      _ => ""
    };
  }
}
