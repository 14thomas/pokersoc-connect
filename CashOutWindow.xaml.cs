using Microsoft.Data.Sqlite;
using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace pokersoc_connect
{
  public partial class CashOutWindow : Window
  {
    // chip cents -> count
    private readonly Dictionary<int,int> _chipCounts = new()
    {
      { 5,0 }, { 25,0 }, { 100,0 }, { 200,0 }, { 500,0 }, { 2500,0 }, { 10000,0 }
    };
    private readonly Stack<int> _history = new();
    private int _multiplier = 1;

    // available cash in cashbox (denom cents -> qty)
    private readonly Dictionary<int,int> _available = new();

    // planned change to give (denom cents -> qty)
    private readonly Dictionary<int,int> _payout = new();

    // expose results to caller (MainWindow)
    public string MemberNumber => ScanBox.Text.Trim();
    public int    TotalCents   => _chipCounts.Sum(kv => kv.Key * kv.Value);
    public double TotalDollars => TotalCents / 100.0;
    public IReadOnlyDictionary<int,int> PayoutPlan => _payout;

    private static readonly int[] DenomsDesc = new[] { 10000, 5000, 2000, 1000, 500, 200, 100, 50, 20, 10, 5 };

    public CashOutWindow()
    {
      InitializeComponent();
      Loaded += (s,e) =>
      {
        LoadAvailableFromDb();
        UpdateAllBadges();
        RecomputePlan();
      };
    }

    // ------- DB: load current cashbox availability -------
    private void LoadAvailableFromDb()
    {
      _available.Clear();
      var sid = Database.ScalarLong("SELECT session_id FROM sessions ORDER BY session_id DESC LIMIT 1");

      var floatDt = Database.Query("SELECT denom_cents, qty FROM cashbox_float WHERE session_id=$s", ("$s", sid));
      foreach (DataRow r in floatDt.Rows)
        _available[Convert.ToInt32(r["denom_cents"])] = Convert.ToInt32(r["qty"]);

      var movDt = Database.Query("SELECT denom_cents, SUM(delta_qty) AS qty FROM cashbox_movements WHERE session_id=$s GROUP BY denom_cents", ("$s", sid));
      foreach (DataRow r in movDt.Rows)
      {
        var d = Convert.ToInt32(r["denom_cents"]);
        var q = Convert.ToInt32(r["qty"]);
        _available[d] = _available.TryGetValue(d, out var cur) ? cur + q : q;
      }

      // ensure all denoms exist
      foreach (var d in DenomsDesc)
        if (!_available.ContainsKey(d)) _available[d] = 0;
    }

    // ------- UI helpers -------
    private IEnumerable<Button> AllChipButtons() => new[] { C5, C25, C100, C200, C500, P2500, P10000 };

    private void UpdateBadgeFor(Button b, int count)
    {
      var badge = (FrameworkElement)b.Template.FindName("Badge", b);
      var text  = (TextBlock)b.Template.FindName("BadgeText", b);
      if (badge != null && text != null)
      {
        if (count > 0) { badge.Visibility = Visibility.Visible; text.Text = count.ToString(); }
        else           { badge.Visibility = Visibility.Collapsed; }
      }
    }

    private void UpdateAllBadges()
    {
      foreach (var b in AllChipButtons())
      {
        int cents = int.Parse(b.Tag.ToString()!);
        _chipCounts.TryGetValue(cents, out var c);
        UpdateBadgeFor(b, c);
      }
    }

    // ------- change planner (greedy, bounded by availability) -------
    private bool TryMakeChange(int amount, Dictionary<int,int> available, out Dictionary<int,int> plan)
    {
      var remaining = amount;
      plan = new Dictionary<int,int>();
      foreach (var d in DenomsDesc)
      {
        if (remaining <= 0) break;
        var want = remaining / d;
        if (want <= 0) continue;
        var take = Math.Min(want, available.TryGetValue(d, out var have) ? have : 0);
        if (take > 0)
        {
          plan[d] = take;
          remaining -= take * d;
        }
      }
      return remaining == 0;
    }

    private void RecomputePlan()
    {
      var culture = CultureInfo.GetCultureInfo("en-AU");
      TotalText.Text = (TotalCents / 100.0).ToString("C", culture);

      _payout.Clear();
      if (TotalCents == 0)
      {
        ChangeGrid.ItemsSource = null;
        ChangeStatus.Text = "Add chips to compute change.";
        ConfirmBtn.IsEnabled = false;
        return;
      }

      // copy availability (don’t mutate original)
      var avail = _available.ToDictionary(kv => kv.Key, kv => kv.Value);

      if (TryMakeChange(TotalCents, avail, out var plan))
      {
        foreach (var kv in plan) _payout[kv.Key] = kv.Value;

        var rows = plan
          .OrderByDescending(kv => kv.Key)
          .Select(kv => new
          {
            Denomination = kv.Key switch {
              5 => "5c", 10 => "10c", 20 => "20c", 50 => "50c",
              100 => "$1", 200 => "$2", 500 => "$5", 1000 => "$10",
              2000 => "$20", 5000 => "$50", 10000 => "$100",
              _ => $"{kv.Key}c"
            },
            Count = kv.Value,
            Value = (kv.Key * kv.Value / 100.0).ToString("C", culture)
          }).ToList();

        ChangeGrid.ItemsSource = rows;
        ChangeStatus.Text = "Exact change available.";
        ConfirmBtn.IsEnabled = true;
      }
      else
      {
        ChangeGrid.ItemsSource = null;
        ChangeStatus.Text = "Cannot make exact change with current cashbox (insufficient small coins/notes).";
        ConfirmBtn.IsEnabled = false;
      }
    }

    // ------- events -------
    private void Multiplier_Click(object sender, RoutedEventArgs e)
    {
      X1.IsChecked = sender == X1;
      X5.IsChecked = sender == X5;
      X10.IsChecked = sender == X10;
      _multiplier = X1.IsChecked == true ? 1 : X5.IsChecked == true ? 5 : 10;
    }

    private void Chip_Click(object sender, RoutedEventArgs e)
    {
      if (sender is not Button b || b.Tag is null || !int.TryParse(b.Tag.ToString(), out var cents)) return;
      var add = Math.Max(1, _multiplier);
      _chipCounts[cents] = _chipCounts.TryGetValue(cents, out var c) ? c + add : add;
      for (int i=0;i<add;i++) _history.Push(cents);
      UpdateBadgeFor(b, _chipCounts[cents]);
      RecomputePlan();
    }

    private void Undo_Click(object sender, RoutedEventArgs e)
    {
      if (_history.Count == 0) return;
      var cents = _history.Pop();
      if (_chipCounts.TryGetValue(cents, out var c) && c > 0) _chipCounts[cents] = c - 1;
      var btn = AllChipButtons().FirstOrDefault(x => x.Tag?.ToString() == cents.ToString());
      if (btn != null) UpdateBadgeFor(btn, _chipCounts[cents]);
      RecomputePlan();
    }

    private void Clear_Click(object sender, RoutedEventArgs e)
    {
      foreach (var k in _chipCounts.Keys.ToList()) _chipCounts[k] = 0;
      _history.Clear();
      UpdateAllBadges();
      RecomputePlan();
    }

    private void ClearScan_Click(object sender, RoutedEventArgs e) => ScanBox.Clear();
    private void Cancel_Click(object sender, RoutedEventArgs e)    => DialogResult = false;

    private void Confirm_Click(object sender, RoutedEventArgs e)
    {
      if (string.IsNullOrWhiteSpace(MemberNumber))
      {
        MessageBox.Show(this, "Please scan or enter a member number first.", "Cash-out");
        ScanBox.Focus(); return;
      }
      if (TotalCents <= 0 || _payout.Count == 0)
      {
        MessageBox.Show(this, "No payout planned.", "Cash-out");
        return;
      }
      DialogResult = true; // caller (MainWindow) will write DB, using MemberNumber, TotalDollars, PayoutPlan
    }

    private void ScanBox_KeyDown(object sender, KeyEventArgs e)
    {
      if (e.Key == Key.Enter && ConfirmBtn.IsEnabled) Confirm_Click(sender, e);
    }
  }
}
