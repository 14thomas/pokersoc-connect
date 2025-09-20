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
      if (TotalCents == 0)
      {
        RefreshChangeTable();
        ConfirmBtn.IsEnabled = false; return;
      }

      var avail = _available.ToDictionary(kv => kv.Key, kv => kv.Value);
      if (TryMakeChange(TotalCents, avail, out var plan))
      {
        foreach (var kv in plan) _payout[kv.Key] = kv.Value;
        RefreshChangeTable();
        ConfirmBtn.IsEnabled = true;
      }
      else
      {
        RefreshChangeTable();
        ConfirmBtn.IsEnabled = false;
      }
    }

    private void RefreshChangeTable()
    {
      var culture = CultureInfo.GetCultureInfo("en-AU");
      ChangeRowsPanel.Children.Clear();

      if (_payout.Count == 0)
      {
        // Show empty state or no change needed
        return;
      }

      var orderedPayout = _payout.OrderByDescending(kv => kv.Key).ToList();
      
      foreach (var kv in orderedPayout)
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

    private void Confirm_Click(object sender, RoutedEventArgs e)
    {
      if (string.IsNullOrWhiteSpace(MemberNumber))
      {
        MessageBox.Show("Please scan or enter a member number first.");
        return;
      }
      if (_payout.Count == 0)
      {
        MessageBox.Show("No payout planned.");
        return;
      }
      // Pass both chips turned in AND cash payout plan
      Confirmed?.Invoke(this, new CashOutConfirmedEventArgs(
        MemberNumber,
        new Dictionary<int,int>(_payout),              // cash plan
        new Dictionary<int,int>(_chipCounts),          // chips turned in
        TotalCents));
    }

    private void ScanBox_KeyDown(object sender, KeyEventArgs e)
    {
      if (e.Key == Key.Enter && ConfirmBtn.IsEnabled) Confirm_Click(sender, e);
    }
  }

  public sealed class CashOutConfirmedEventArgs : EventArgs
  {
    public string MemberNumber { get; }
    public Dictionary<int,int> PayoutPlan { get; }   // cash paid out (AU denoms)
    public Dictionary<int,int> ChipsIn { get; }      // chips turned in
    public int TotalCents { get; }
    public CashOutConfirmedEventArgs(string member,
                                     Dictionary<int,int> plan,
                                     Dictionary<int,int> chipsIn,
                                     int totalCents)
      => (MemberNumber, PayoutPlan, ChipsIn, TotalCents) = (member, plan, chipsIn, totalCents);
  }
}
