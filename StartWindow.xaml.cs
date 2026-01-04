using Microsoft.Win32;
using System.Windows;

namespace pokersoc_connect
{
  public partial class StartWindow : Window
  {
    public string? SelectedPath { get; private set; }
    public string? PlayerImportPath { get; private set; }
    public bool AutoImportPlayers => AutoImportCheck.IsChecked == true;

    public StartWindow()
    {
      InitializeComponent();
    }

    private void New_Click(object sender, RoutedEventArgs e)
    {
      var dlg = new SaveFileDialog {
        Filter = "Session (*.sqlite)|*.sqlite|All files|*.*",
        FileName = $"Session-{System.DateTime.Now:yyyy-MM-dd}.sqlite"
      };
      if (dlg.ShowDialog() == true) {
        SelectedPath = dlg.FileName;
        DialogResult = true;
      }
    }

    private void Open_Click(object sender, RoutedEventArgs e)
    {
      var dlg = new OpenFileDialog {
        Filter = "Session (*.sqlite)|*.sqlite|All files|*.*"
      };
      if (dlg.ShowDialog() == true) {
        SelectedPath = dlg.FileName;
        DialogResult = true;
      }
    }

    private void ImportPlayers_Click(object sender, RoutedEventArgs e)
    {
      var dlg = new OpenFileDialog {
        Title = "Select Player Database to Import",
        Filter = "CSV Files (*.csv)|*.csv|All files|*.*"
      };
      if (dlg.ShowDialog() == true) {
        PlayerImportPath = dlg.FileName;
        MessageBox.Show($"Player database selected:\n{dlg.FileName}\n\nPlayers will be imported when you open or create a session.", 
          "Import Ready", MessageBoxButton.OK, MessageBoxImage.Information);
      }
    }

    private void Exit_Click(object sender, RoutedEventArgs e) => Close();
  }
}
