using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace pokersoc_connect
{
  public partial class BuyInWindow : Window
  {
    // cents -> count
    private readonly Dictionary<int,int> _counts = new()
    {
      { 5,0 }, {10,0}, {20,0}, {50,0},
      {100,0}, {200,0},
      {500,0}, {1000,0}, {2000,0}, {5000,0}, {10000,0}
    };
    private readonly Stack<int> _history = new();
    private int _multiplier = 1;

    public string MemberNumber => ScanBox.Text.Trim();
    public int TotalCents => _counts.Sum(kv => kv.Key * kv.Value);
    public double TotalDollars => TotalCents / 100.0;
    public IReadOnlyDictionary<int,int> DenomCounts => _counts;

    public BuyInWindow()
    {
      InitializeComponent();
      Loaded += (s,e) => { ScanBox.Focus(); UpdateAllBadges(); UpdateTotal(); };
    }

    private void Multiplier_Click(object sender, RoutedEventArgs e)
    {
      X1.IsChecked = sender == X1;
      X5.IsChecked = sender == X5;
      X10.IsChecked = sender == X10;
      _multiplier = X1.IsChecked == true ? 1 : X5.IsChecked == true ? 5 : 10;
    }

    private void Denom_Click(object sender, RoutedEventArgs e)
    {
      if (sender is not Button b || b.Tag is null || !int.TryParse(b.Tag.ToString(), out var cents)) return;
      var add = Math.Max(1, _multiplier);
      _counts[cents] = _counts.TryGetValue(cents, out var c) ? c + add : add;
      for (int i=0;i<add;i++) _history.Push(cents);
      UpdateBadgeFor(b, _counts[cents]);
      UpdateTotal();
    }

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

    private IEnumerable<Button> AllDenomButtons()
    {
      return new[] { B5,B10,B20,B50,B100,B200,B500,B1000,B2000,B5000,B10000 };
    }

    private void UpdateAllBadges()
    {
      foreach (var b in AllDenomButtons())
      {
        int cents = int.Parse(b.Tag.ToString()!);
        UpdateBadgeFor(b, _counts.TryGetValue(cents, out var c) ? c : 0);
      }
    }

    private void UpdateTotal()
      => TotalText.Text = TotalDollars.ToString("C", CultureInfo.GetCultureInfo("en-AU"));

    private void Undo_Click(object sender, RoutedEventArgs e)
    {
      if (_history.Count == 0) return;
      var cents = _history.Pop();
      if (_counts.TryGetValue(cents, out var c) && c > 0) _counts[cents] = c - 1;
      // find the button by tag
      var button = AllDenomButtons().FirstOrDefault(b => b.Tag?.ToString() == cents.ToString());
      if (button != null) UpdateBadgeFor(button, _counts[cents]);
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

    private void Cancel_Click(object sender, RoutedEventArgs e) => DialogResult = false;

    private void Confirm_Click(object sender, RoutedEventArgs e)
    {
      if (string.IsNullOrWhiteSpace(MemberNumber))
      {
        MessageBox.Show(this, "Please scan or enter a member number first.", "Buy-in");
        ScanBox.Focus();
        return;
      }
      if (TotalCents <= 0)
      {
        MessageBox.Show(this, "Please add at least one coin/note.", "Buy-in");
        return;
      }
      DialogResult = true;
    }

    private void ScanBox_KeyDown(object sender, KeyEventArgs e)
    {
      if (e.Key == Key.Enter) Confirm_Click(sender, e);
    }
  }
}
