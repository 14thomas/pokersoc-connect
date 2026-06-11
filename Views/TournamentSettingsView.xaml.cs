using System;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;

namespace pokersoc_connect.Views
{
  public partial class TournamentSettingsView : UserControl
  {
    public event EventHandler? CloseRequested;
    public event EventHandler? SettingsSaved;

    public TournamentSettingsView()
    {
      InitializeComponent();
      LoadSettings();
    }

    private void LoadSettings()
    {
      var settings = Database.GetTournamentSettings();
      ArcBuyInBox.Text = (settings.ArcMemberBuyInCents / 100.0).ToString("F2", CultureInfo.InvariantCulture);
      NonArcBuyInBox.Text = (settings.NonArcMemberBuyInCents / 100.0).ToString("F2", CultureInfo.InvariantCulture);
      ArcRebuyBox.Text = (settings.ArcMemberRebuyCents / 100.0).ToString("F2", CultureInfo.InvariantCulture);
      NonArcRebuyBox.Text = (settings.NonArcMemberRebuyCents / 100.0).ToString("F2", CultureInfo.InvariantCulture);
      ArcOnlyCheck.IsChecked = settings.ArcOnly;

      var unlimited = settings.RebuyCap <= 0;
      UnlimitedRebuysCheck.IsChecked = unlimited;
      RebuyCapBox.Text = unlimited ? "2" : settings.RebuyCap.ToString(CultureInfo.InvariantCulture);
      RebuyCapBox.IsEnabled = !unlimited;
    }

    private void UnlimitedRebuysCheck_Changed(object sender, RoutedEventArgs e)
    {
      if (RebuyCapBox == null) return;
      RebuyCapBox.IsEnabled = UnlimitedRebuysCheck.IsChecked != true;
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
      if (!TryParseDollars(ArcBuyInBox.Text, out var arcDollars) ||
          !TryParseDollars(NonArcBuyInBox.Text, out var nonArcDollars) ||
          !TryParseDollars(ArcRebuyBox.Text, out var arcRebuyDollars) ||
          !TryParseDollars(NonArcRebuyBox.Text, out var nonArcRebuyDollars))
      {
        MessageBox.Show("Please enter valid buy-in and rebuy amounts.", "Validation Error",
          MessageBoxButton.OK, MessageBoxImage.Warning);
        return;
      }

      var unlimited = UnlimitedRebuysCheck.IsChecked == true;
      int rebuyCap = 0;
      if (!unlimited)
      {
        if (!int.TryParse(RebuyCapBox.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out rebuyCap) ||
            rebuyCap < 1)
        {
          MessageBox.Show("Please enter a valid rebuy cap (1 or more), or enable unlimited rebuys.",
            "Validation Error", MessageBoxButton.OK, MessageBoxImage.Warning);
          return;
        }
      }

      var settings = new TournamentSettings
      {
        ArcMemberBuyInCents = (int)Math.Round(arcDollars * 100),
        NonArcMemberBuyInCents = (int)Math.Round(nonArcDollars * 100),
        ArcMemberRebuyCents = (int)Math.Round(arcRebuyDollars * 100),
        NonArcMemberRebuyCents = (int)Math.Round(nonArcRebuyDollars * 100),
        ArcOnly = ArcOnlyCheck.IsChecked == true,
        RebuyCap = rebuyCap
      };

      try
      {
        Database.SetTournamentSettings(settings);
        SettingsSaved?.Invoke(this, EventArgs.Empty);
        CloseRequested?.Invoke(this, EventArgs.Empty);
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Could not save settings:\n{ex.Message}", "Error",
          MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private static bool TryParseDollars(string text, out double dollars)
    {
      dollars = 0;
      if (!double.TryParse(text, NumberStyles.Number, CultureInfo.InvariantCulture, out dollars) &&
          !double.TryParse(text, NumberStyles.Number, CultureInfo.CurrentCulture, out dollars))
        return false;
      return dollars >= 0;
    }

    private void Close_Click(object sender, RoutedEventArgs e) => CloseRequested?.Invoke(this, EventArgs.Empty);
  }
}
