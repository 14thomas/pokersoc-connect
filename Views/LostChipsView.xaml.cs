using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace pokersoc_connect.Views
{
  public partial class LostChipsView : UserControl
  {
    // Poker chip denominations only
    private static readonly int[] PokerChipDenoms = { 5, 25, 100, 200, 500, 2500, 10000 };
    private readonly Dictionary<int, TextBlock> _chipCounts = new Dictionary<int, TextBlock>();
    private readonly Dictionary<int, int> _chipValues = new Dictionary<int, int>();
    private int _currentMultiplier = 1; // Current multiplier (1, 5, or 20)
    private readonly List<(int denom, int count)> _undoStack = new List<(int, int)>(); // For undo functionality

    public event EventHandler? CloseRequested;

    public LostChipsView()
    {
      InitializeComponent();
      InitializeChipCounts();
      UpdateTotal();
      
      // Set default multiplier to 1x
      Toggle1x.IsChecked = true;
      _currentMultiplier = 1;
    }

    private void InitializeChipCounts()
    {
      _chipCounts[5] = Chip5c;    // White 5c
      _chipCounts[25] = Chip25c;  // Red 25c
      _chipCounts[100] = Chip1;   // Blue $1
      _chipCounts[200] = Chip2;   // Green $2
      _chipCounts[500] = Chip5;   // Black $5
      _chipCounts[2500] = Chip25; // White Plaque $25
      _chipCounts[10000] = Chip100; // Red Plaque $100

      // Initialize all values to 0
      foreach (var denom in PokerChipDenoms)
      {
        _chipValues[denom] = 0;
      }
    }

    private void AddChip(int denom, int count = 1)
    {
      // Save current state for undo
      _undoStack.Add((denom, _chipValues[denom]));
      
      _chipValues[denom] += count * _currentMultiplier;
      _chipCounts[denom].Text = _chipValues[denom].ToString();
      UpdateTotal();
    }

    private void UpdateTotal()
    {
      double totalCents = _chipValues.Sum(kvp => kvp.Value * kvp.Key);
      var au = CultureInfo.GetCultureInfo("en-AU");
      TotalTipsText.Text = $"Total Tips: {(totalCents / 100.0).ToString("C", au)}";
    }

    // Add chip handlers - one click adds chips based on current multiplier
    private void AddChip5c_Click(object sender, RoutedEventArgs e) => AddChip(5);
    private void AddChip25c_Click(object sender, RoutedEventArgs e) => AddChip(25);
    private void AddChip1_Click(object sender, RoutedEventArgs e) => AddChip(100);
    private void AddChip2_Click(object sender, RoutedEventArgs e) => AddChip(200);
    private void AddChip5_Click(object sender, RoutedEventArgs e) => AddChip(500);
    private void AddChip25_Click(object sender, RoutedEventArgs e) => AddChip(2500);
    private void AddChip100_Click(object sender, RoutedEventArgs e) => AddChip(10000);

    // Toggle button handlers
    private void Toggle1x_Click(object sender, RoutedEventArgs e)
    {
      if (Toggle1x.IsChecked == true)
      {
        Toggle5x.IsChecked = false;
        Toggle20x.IsChecked = false;
        _currentMultiplier = 1;
      }
      else
      {
        _currentMultiplier = 1; // Default to 1x if all are unchecked
      }
    }

    private void Toggle5x_Click(object sender, RoutedEventArgs e)
    {
      if (Toggle5x.IsChecked == true)
      {
        Toggle1x.IsChecked = false;
        Toggle20x.IsChecked = false;
        _currentMultiplier = 5;
      }
      else
      {
        _currentMultiplier = 1; // Default to 1x if all are unchecked
      }
    }

    private void Toggle20x_Click(object sender, RoutedEventArgs e)
    {
      if (Toggle20x.IsChecked == true)
      {
        Toggle1x.IsChecked = false;
        Toggle5x.IsChecked = false;
        _currentMultiplier = 20;
      }
      else
      {
        _currentMultiplier = 1; // Default to 1x if all are unchecked
      }
    }

    private void RecordTips_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        var tipsToRecord = _chipValues.Where(kvp => kvp.Value > 0).ToList();

        if (!tipsToRecord.Any())
        {
          MessageBox.Show("Please click on at least one chip to record as tips.", "No Tips", 
            MessageBoxButton.OK, MessageBoxImage.Information);
          return;
        }

        var totalValue = tipsToRecord.Sum(t => t.Value * t.Key);
        var totalChips = tipsToRecord.Sum(t => t.Value);

        var result = MessageBox.Show(
          $"Record {totalChips} lost chips as tips?\n\n" +
          $"Total value: {(totalValue / 100.0).ToString("C", CultureInfo.GetCultureInfo("en-AU"))}",
          "Confirm Lost Chips", MessageBoxButton.YesNo, MessageBoxImage.Question);

        if (result == MessageBoxResult.Yes)
        {
          Database.InTransaction(tx =>
          {
            foreach (var (denom, qty) in tipsToRecord)
            {
              // Record as tip
              Database.Exec(
                "INSERT INTO tips(denom_cents, qty, notes) VALUES ($d, $q, $n)",
                tx, ("$d", denom), ("$q", qty), ("$n", "Lost chip recorded as tip")
              );

              // Record as cashbox movement (positive for tips)
              Database.Exec(
                "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, notes) VALUES ($d, $q, 'LOST_CHIP', $n)",
                tx, ("$d", denom), ("$q", qty), ("$n", "Lost chip recorded as tip")
              );
            }
          });

          MessageBox.Show("Lost chips recorded as tips successfully!", "Success", 
            MessageBoxButton.OK, MessageBoxImage.Information);
          
          ClearAll_Click(sender, e);
        }
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error recording lost chips: {ex.Message}", "Error", 
          MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void UndoLast_Click(object sender, RoutedEventArgs e)
    {
      if (_undoStack.Count > 0)
      {
        var (denom, previousCount) = _undoStack[_undoStack.Count - 1];
        _undoStack.RemoveAt(_undoStack.Count - 1);
        
        _chipValues[denom] = previousCount;
        _chipCounts[denom].Text = _chipValues[denom].ToString();
        UpdateTotal();
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
      
      foreach (var denom in PokerChipDenoms)
      {
        _chipValues[denom] = 0;
        _chipCounts[denom].Text = "0";
      }
      UpdateTotal();
    }

    private void Close_Click(object sender, RoutedEventArgs e)
    {
      CloseRequested?.Invoke(this, EventArgs.Empty);
    }
  }
}