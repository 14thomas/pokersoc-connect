using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

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

    public FloatWindow(Dictionary<int,int>? preset = null)
    {
      InitializeComponent();
      if (preset != null)
        foreach (var kv in preset) if (Counts.ContainsKey(kv.Key)) Counts[kv.Key] = kv.Value;

      Loaded += (s,e) => { UpdateAllBadges(); UpdateTotal(); };
    }

    private IEnumerable<Button> AllButtons() =>
      new[] { B5,B10,B20,B50,B100,B200,B500,B1000,B2000,B5000,B10000 };

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
      foreach (var b in AllButtons())
      {
        int cents = int.Parse(b.Tag.ToString()!);
        UpdateBadgeFor(b, Counts.TryGetValue(cents, out var c) ? c : 0);
      }
    }

    private void UpdateTotal()
    {
      var cents = Counts.Sum(kv => kv.Key * kv.Value);
      TotalText.Text = (cents / 100.0).ToString("C", CultureInfo.GetCultureInfo("en-AU"));
    }

    private void Denom_Click(object sender, RoutedEventArgs e)
    {
      if (sender is not Button b || b.Tag is null || !int.TryParse(b.Tag.ToString(), out var cents)) return;
      var add = _multiplier <= 0 ? 1 : _multiplier;
      Counts[cents] = Counts.TryGetValue(cents, out var c) ? c + add : add;
      UpdateBadgeFor(b, Counts[cents]);
      UpdateTotal();
    }

    private void Multiplier_Click(object sender, RoutedEventArgs e)
    {
      X1.IsChecked = sender == X1;
      X5.IsChecked = sender == X5;
      X10.IsChecked = sender == X10;
      _multiplier = X1.IsChecked == true ? 1 : X5.IsChecked == true ? 5 : 10;
    }

    private void Clear_Click(object sender, RoutedEventArgs e)
    {
      foreach (var k in Counts.Keys.ToList()) Counts[k] = 0;
      UpdateAllBadges();
      UpdateTotal();
    }

    private void Save_Click(object sender, RoutedEventArgs e) => DialogResult = true;

    private void Cancel_Click(object sender, RoutedEventArgs e) => DialogResult = false;
  }
}
