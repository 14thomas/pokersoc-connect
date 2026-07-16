using System;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace pokersoc_connect.Views
{
  public partial class TournamentPricingView : UserControl
  {
    public event EventHandler? CloseRequested;
    public event EventHandler? PricingSaved;

    private TournamentPricingStructure _selectedStructure = TournamentPricingStructure.ArcVsNonArc;

    private static readonly Brush SelectedBorder = new SolidColorBrush(Color.FromRgb(0x2E, 0x6D, 0xA4));
    private static readonly Brush SelectedBg = new SolidColorBrush(Color.FromRgb(0xE8, 0xF4, 0xFD));
    private static readonly Brush UnselectedBorder = Brushes.Gray;
    private static readonly Brush UnselectedBg = new SolidColorBrush(Color.FromRgb(0xF5, 0xF5, 0xF5));

    public TournamentPricingView()
    {
      InitializeComponent();
      LoadSettings();
    }

    private void LoadSettings()
    {
      var settings = Database.GetTournamentSettings();
      _selectedStructure = settings.PricingStructure;

      ArcBuyInBox.Text = Dollars(settings.ArcMemberBuyInCents);
      NonArcBuyInBox.Text = Dollars(settings.NonArcMemberBuyInCents);
      ArcRebuyBox.Text = Dollars(settings.ArcMemberRebuyCents);
      NonArcRebuyBox.Text = Dollars(settings.NonArcMemberRebuyCents);
      ArcOnlyCheck.IsChecked = settings.ArcOnly;

      BronzeBuyInBox.Text = Dollars(settings.BronzeBuyInCents);
      SilverBuyInBox.Text = Dollars(settings.SilverBuyInCents);
      GoldBuyInBox.Text = Dollars(settings.GoldBuyInCents);
      PlatinumBuyInBox.Text = Dollars(settings.PlatinumBuyInCents);
      DiamondBuyInBox.Text = Dollars(settings.DiamondBuyInCents);

      BronzeRebuyBox.Text = Dollars(settings.BronzeRebuyCents);
      SilverRebuyBox.Text = Dollars(settings.SilverRebuyCents);
      GoldRebuyBox.Text = Dollars(settings.GoldRebuyCents);
      PlatinumRebuyBox.Text = Dollars(settings.PlatinumRebuyCents);
      DiamondRebuyBox.Text = Dollars(settings.DiamondRebuyCents);

      ApplyStructureUi();
    }

    private static string Dollars(int cents)
      => (cents / 100.0).ToString("F2", CultureInfo.InvariantCulture);

    private void ArcStructure_Click(object sender, RoutedEventArgs e)
    {
      _selectedStructure = TournamentPricingStructure.ArcVsNonArc;
      ApplyStructureUi();
    }

    private void MembershipStructure_Click(object sender, RoutedEventArgs e)
    {
      _selectedStructure = TournamentPricingStructure.MembershipTiers;
      ApplyStructureUi();
    }

    private void ApplyStructureUi()
    {
      var isArc = _selectedStructure == TournamentPricingStructure.ArcVsNonArc;
      ArcPricingPanel.Visibility = isArc ? Visibility.Visible : Visibility.Collapsed;
      MembershipPricingPanel.Visibility = isArc ? Visibility.Collapsed : Visibility.Visible;

      ArcStructureButton.BorderBrush = isArc ? SelectedBorder : UnselectedBorder;
      ArcStructureButton.Background = isArc ? SelectedBg : UnselectedBg;
      MembershipStructureButton.BorderBrush = isArc ? UnselectedBorder : SelectedBorder;
      MembershipStructureButton.Background = isArc ? UnselectedBg : SelectedBg;
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
      var existing = Database.GetTournamentSettings();

      if (_selectedStructure == TournamentPricingStructure.ArcVsNonArc)
      {
        if (!TryParseDollars(ArcBuyInBox.Text, out var arcBuyIn) ||
            !TryParseDollars(NonArcBuyInBox.Text, out var nonArcBuyIn) ||
            !TryParseDollars(ArcRebuyBox.Text, out var arcRebuy) ||
            !TryParseDollars(NonArcRebuyBox.Text, out var nonArcRebuy))
        {
          MessageBox.Show("Please enter valid ARC buy-in and rebuy amounts.", "Validation Error",
            MessageBoxButton.OK, MessageBoxImage.Warning);
          return;
        }

        existing.PricingStructure = TournamentPricingStructure.ArcVsNonArc;
        existing.ArcMemberBuyInCents = ToCents(arcBuyIn);
        existing.NonArcMemberBuyInCents = ToCents(nonArcBuyIn);
        existing.ArcMemberRebuyCents = ToCents(arcRebuy);
        existing.NonArcMemberRebuyCents = ToCents(nonArcRebuy);
        existing.ArcOnly = ArcOnlyCheck.IsChecked == true;
      }
      else
      {
        if (!TryParseDollars(BronzeBuyInBox.Text, out var bronzeBuyIn) ||
            !TryParseDollars(SilverBuyInBox.Text, out var silverBuyIn) ||
            !TryParseDollars(GoldBuyInBox.Text, out var goldBuyIn) ||
            !TryParseDollars(PlatinumBuyInBox.Text, out var platinumBuyIn) ||
            !TryParseDollars(DiamondBuyInBox.Text, out var diamondBuyIn) ||
            !TryParseDollars(BronzeRebuyBox.Text, out var bronzeRebuy) ||
            !TryParseDollars(SilverRebuyBox.Text, out var silverRebuy) ||
            !TryParseDollars(GoldRebuyBox.Text, out var goldRebuy) ||
            !TryParseDollars(PlatinumRebuyBox.Text, out var platinumRebuy) ||
            !TryParseDollars(DiamondRebuyBox.Text, out var diamondRebuy))
        {
          MessageBox.Show("Please enter valid membership buy-in and rebuy amounts.", "Validation Error",
            MessageBoxButton.OK, MessageBoxImage.Warning);
          return;
        }

        existing.PricingStructure = TournamentPricingStructure.MembershipTiers;
        existing.BronzeBuyInCents = ToCents(bronzeBuyIn);
        existing.SilverBuyInCents = ToCents(silverBuyIn);
        existing.GoldBuyInCents = ToCents(goldBuyIn);
        existing.PlatinumBuyInCents = ToCents(platinumBuyIn);
        existing.DiamondBuyInCents = ToCents(diamondBuyIn);
        existing.BronzeRebuyCents = ToCents(bronzeRebuy);
        existing.SilverRebuyCents = ToCents(silverRebuy);
        existing.GoldRebuyCents = ToCents(goldRebuy);
        existing.PlatinumRebuyCents = ToCents(platinumRebuy);
        existing.DiamondRebuyCents = ToCents(diamondRebuy);
        existing.ArcOnly = false;
      }

      try
      {
        Database.SetTournamentSettings(existing);
        PricingSaved?.Invoke(this, EventArgs.Empty);
        CloseRequested?.Invoke(this, EventArgs.Empty);
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Could not save pricing:\n{ex.Message}", "Error",
          MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private static int ToCents(double dollars) => (int)Math.Round(dollars * 100);

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
