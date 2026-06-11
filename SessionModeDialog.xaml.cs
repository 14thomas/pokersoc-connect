using System.Windows;

namespace pokersoc_connect
{
  public partial class SessionModeDialog : Window
  {
    public SessionMode SelectedMode { get; private set; } = SessionMode.CashGame;

    public SessionModeDialog()
    {
      InitializeComponent();
    }

    private void Ok_Click(object sender, RoutedEventArgs e)
    {
      SelectedMode = TournamentRadio.IsChecked == true ? SessionMode.Tournament : SessionMode.CashGame;
      DialogResult = true;
    }

    private void Cancel_Click(object sender, RoutedEventArgs e) => DialogResult = false;
  }
}
