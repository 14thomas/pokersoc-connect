using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace pokersoc_connect.Views
{
  public partial class TournamentInfoView : UserControl
  {
    public event EventHandler? CloseRequested;

    public TournamentInfoView()
    {
      InitializeComponent();
      RefreshStats();
    }

    public void RefreshStats()
    {
      var (buyIns, rebuys) = Database.GetTournamentSessionStats();
      TotalBuyInsText.Text = buyIns.ToString();
      TotalRebuysText.Text = rebuys.ToString();

      BuyInBreakdownPanel.Children.Clear();
      RebuyBreakdownPanel.Children.Clear();

      foreach (var (tier, tierBuyIns, tierRebuys) in Database.GetTournamentMembershipStats())
      {
        if (tierBuyIns > 0)
          BuyInBreakdownPanel.Children.Add(CreateBreakdownRow(tier, tierBuyIns, "#2E6DA4"));
        if (tierRebuys > 0)
          RebuyBreakdownPanel.Children.Add(CreateBreakdownRow(tier, tierRebuys, "#B8860B"));
      }
    }

    private static TextBlock CreateBreakdownRow(string tier, int count, string colorHex)
    {
      return new TextBlock
      {
        Text = $"{tier}: {count}",
        FontSize = 14,
        FontWeight = FontWeights.SemiBold,
        HorizontalAlignment = HorizontalAlignment.Center,
        Margin = new Thickness(0, 2, 0, 2),
        Foreground = (Brush)new BrushConverter().ConvertFromString(colorHex)!
      };
    }

    private void Close_Click(object sender, RoutedEventArgs e)
      => CloseRequested?.Invoke(this, EventArgs.Empty);
  }
}
