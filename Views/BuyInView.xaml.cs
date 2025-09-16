using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace pokersoc_connect.Views
{
  public partial class BuyInView : UserControl
  {
    public event EventHandler<BuyInConfirmedEventArgs>? Confirmed;
    public event EventHandler? Cancelled;

    private readonly Dictionary<int,int> _counts = new()
    {
      { 5,0 }, {10,0}, {20,0}, {50,0},
      {100,0}, {200,0},
      {500,0}, {1000,0}, {2000,0}, {5000,0}, {10000,0}
    };
    private readonly Stack<int> _history = new();
    private int _multiplier = 1;

    public BuyInView()
    {
      InitializeComponent();
      Loaded += (s,e) => { UpdateAllBadges(); UpdateTotal(); ScanBox.Focus(); };
    }

    public string MemberNumber => ScanBox.Text.Trim();
    public int TotalCents => _counts.Sum(kv => kv.Key * kv.Value);
    public double TotalDollars => TotalCents / 100.0;
    public IReadOnlyDictionary<int,int> DenomCounts => _counts;

    private IEnumerable<Button> AllButtons() =>
      new[] { B5,B10,B20,B50,B100,B200,B500,B1000,B2000,B5000,B10000 };

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
      foreach (var b in AllButtons())
      {
        int cents = int.Parse(b.Tag.ToString()!);
        _counts.TryGetValue(cents, out var c);
        UpdateBadgeFor(b, c);
      }
    }

    private void UpdateTotal()
      => TotalText.Text = (TotalCents / 100.0).ToString("C", CultureInfo.GetCultureInfo("en-AU"));

    private void Multiplier_Click(object sender, RoutedEventArgs e)
    {
      X1.IsChecked = sender == X1;
      X5.IsChecked = sender == X5;
      X10.IsChecked = sender == X10;
      _multiplier = X1.IsChecked == true ? 1 : X5.IsChecked == true ? 5 : 10;
    }

    private void Denom_Click(object sender, RoutedEventArgs e)
    {
      if (sender is not Button b || !int.TryParse(b.Tag?.ToString(), out var cents)) return;
      var add = Math.Max(1, _multiplier);
      _counts[cents] = _counts.TryGetValue(cents, out var c) ? c + add : add;
      for (int i=0;i<add;i++) _history.Push(cents);
      UpdateBadgeFor(b, _counts[cents]);
      UpdateTotal();
    }

    private void Undo_Click(object sender, RoutedEventArgs e)
    {
      if (_history.Count == 0) return;
      var cents = _history.Pop();
      if (_counts[cents] > 0) _counts[cents]--;
      var btn = AllButtons().FirstOrDefault(x => x.Tag?.ToString()==cents.ToString());
      if (btn != null) UpdateBadgeFor(btn, _counts[cents]);
      UpdateTotal();
    }

    private void Clear_Click(object sender, RoutedEventArgs e)
    {
      foreach (var k in _counts.Keys.ToList()) _counts[k] = 0;
      _history.Clear();
      UpdateAllBadges();
      UpdateTotal();
    }

    private void ClearScan_Click(object sender, RoutedEventArgs e) => ScanBox.Clear();

    private void Cancel_Click(object sender, RoutedEventArgs e) => Cancelled?.Invoke(this, EventArgs.Empty);

    private void Confirm_Click(object sender, RoutedEventArgs e)
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
      Confirmed?.Invoke(this, new BuyInConfirmedEventArgs(MemberNumber, new Dictionary<int,int>(_counts), TotalCents));
    }

    private void ScanBox_KeyDown(object sender, KeyEventArgs e)
    {
      if (e.Key == Key.Enter) Confirm_Click(sender, e);
    }
  }

  public sealed class BuyInConfirmedEventArgs : EventArgs
  {
    public string MemberNumber { get; }
    public Dictionary<int,int> DenomCounts { get; }
    public int TotalCents { get; }
    public BuyInConfirmedEventArgs(string member, Dictionary<int,int> denoms, int totalCents)
      => (MemberNumber, DenomCounts, TotalCents) = (member, denoms, totalCents);
  }
}
