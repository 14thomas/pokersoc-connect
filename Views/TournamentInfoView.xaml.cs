using System;
using System.Windows;
using System.Windows.Controls;

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
    }

    private void Close_Click(object sender, RoutedEventArgs e)
      => CloseRequested?.Invoke(this, EventArgs.Empty);
  }
}
