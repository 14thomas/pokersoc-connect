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
    public event EventHandler? ConfigurePricingRequested;

    public TournamentSettingsView()
    {
      InitializeComponent();
      LoadSettings();
    }

    public void RefreshPricingSummary()
    {
      var settings = Database.GetTournamentSettings();
      PricingStructureSummary.Text = $"Current: {settings.GetPricingStructureSummary()}";
    }

    private void LoadSettings()
    {
      var settings = Database.GetTournamentSettings();
      PricingStructureSummary.Text = $"Current: {settings.GetPricingStructureSummary()}";

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

    private void ConfigurePricing_Click(object sender, RoutedEventArgs e)
      => ConfigurePricingRequested?.Invoke(this, EventArgs.Empty);

    private void Save_Click(object sender, RoutedEventArgs e)
    {
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

      try
      {
        var settings = Database.GetTournamentSettings();
        settings.RebuyCap = rebuyCap;
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

    private void Close_Click(object sender, RoutedEventArgs e) => CloseRequested?.Invoke(this, EventArgs.Empty);
  }
}
