using System;
using System.Windows;

namespace pokersoc_connect
{
  public partial class App : Application
  {
    protected override void OnStartup(StartupEventArgs e)
    {
      base.OnStartup(e);

      // Prevent the app from exiting when StartWindow closes
      var prevMode = this.ShutdownMode;
      this.ShutdownMode = ShutdownMode.OnExplicitShutdown;

      // Show the start picker (New/Open)
      var start = new StartWindow();
      bool? ok = start.ShowDialog();

      if (ok != true || string.IsNullOrWhiteSpace(start.SelectedPath))
      {
        Shutdown();
        return;
      }

      try
      {
        Database.Open(start.SelectedPath); // creates file if new and applies schema
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Could not open database:\n{ex.Message}", "pokersoc-connect",
          MessageBoxButton.OK, MessageBoxImage.Error);
        Shutdown();
        return;
      }

      // Create and show the real main window
      var main = new MainWindow();
      this.MainWindow = main;
      main.Show();

      // Now normal shutdown behavior: exit when MainWindow closes
      this.ShutdownMode = ShutdownMode.OnMainWindowClose;
    }
  }
}
