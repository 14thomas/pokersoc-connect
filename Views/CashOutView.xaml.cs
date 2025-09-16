using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace pokersoc_connect.Views
{
  public partial class CashOutView : UserControl
  {
    public event EventHandler<CashOutConfirmedEventArgs>? Confirmed;
    public event EventHandler? Back;

    private readonly Dictionary<int,int> _chipCounts = new()
      { {5,0},{25,0},{100,0},{200,0},{500,0},{2500,0},{10000,0} };

    private readonly Stack<int> _history = new();
    private int _multiplier = 1;

    private readonly Dictionary<int,int> _available = new();
    private readonly Dictionary<int,int> _payout = new();
    private static readonly int[] DenomsDesc = { 10000,5000,2000,1000,500,200,100,50,20,10,5 };

    public string MemberNumber => ScanBox.Text.Trim();
    public int TotalCents => _chipCounts.Sum(kv => kv.Key * kv.Value);
    public double TotalDollars => TotalCents / 100.0;
    public IReadOnlyDictionary<int,int> PayoutPlan => _payout;

    public CashOutView()
    {
      InitializeComponent();
      Loaded += (s,e) => { LoadAvailableFromDb(); UpdateAllBadges(); RecomputePlan(); ScanBox.Focus(); };
    }

    private void LoadAvailableFromDb()
    {
      _available.Clear();

      var f = Database.Query("SELECT denom_cents, qty FROM cashbox_float");
      foreach (DataRow r in f.Rows) _available[Convert.ToInt32(r["denom_cents"])] = Convert.ToInt32(r["qty"]);

      var m = Database.Query("SELECT denom_cents, SUM(delta_qty) AS qty FROM cashbox_movements GROUP BY denom_cents");
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
        ChangeGrid.ItemsSource = null;
        ChangeStatus.Text = "Add chips to compute change.";
        ConfirmBtn.IsEnabled = false; return;
      }

      var avail = _available.ToDictionary(kv => kv.Key, kv => kv.Value);
      if (TryMakeChange(TotalCents, avail, out var plan))
      {
        foreach (var kv in plan) _payout[kv.Key] = kv.Value;
        ChangeGrid.ItemsSource = plan.OrderByDescending(kv => kv.Key)
          .Select(kv => new {
            Denomination = kv.Key switch {
              5=>"5c",10=>"10c",20=>"20c",50=>"50c",
              100=>"$1",200=>"$2",500=>"$5",1000=>"$10",
              2000=>"$20",5000=>"$50",10000=>"$100", _=>$"{kv.Key}c"
            },
            Count = kv.Value,
            Value = (kv.Key*kv.Value/100.0).ToString("C", culture)
          }).ToList();
        ChangeStatus.Text = "Exact change available.";
        ConfirmBtn.IsEnabled = true;
      }
      else
      {
        ChangeGrid.ItemsSource = null;
        ChangeStatus.Text = "Cannot make exact change with current cashbox.";
        ConfirmBtn.IsEnabled = false;
      }
    }

    private void Multiplier_Click(object sender, RoutedEventArgs e)
    {
      X1.IsChecked = sender == X1; X5.IsChecked = sender == X5; X10.IsChecked = sender == X10;
      _multiplier = X1.IsChecked == true ? 1 : X5.IsChecked == true ? 5 : 10;
    }

    private void Chip_Click(object sender, RoutedEventArgs e)
    {
      if (sender is not Button b || !int.TryParse(b.Tag?.ToString(), out var cents)) return;
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
      if (_chipCounts[cents] > 0) _chipCounts[cents]--;
      var btn = AllChipButtons().FirstOrDefault(x => x.Tag?.ToString()==cents.ToString());
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
      Confirmed?.Invoke(this, new CashOutConfirmedEventArgs(MemberNumber, new Dictionary<int,int>(_payout), TotalCents));
    }

    private void ScanBox_KeyDown(object sender, KeyEventArgs e)
    {
      if (e.Key == Key.Enter && ConfirmBtn.IsEnabled) Confirm_Click(sender, e);
    }
  }

  public sealed class CashOutConfirmedEventArgs : EventArgs
  {
    public string MemberNumber { get; }
    public Dictionary<int,int> PayoutPlan { get; }
    public int TotalCents { get; }
    public CashOutConfirmedEventArgs(string member, Dictionary<int,int> plan, int totalCents)
      => (MemberNumber, PayoutPlan, TotalCents) = (member, plan, totalCents);
  }
}
