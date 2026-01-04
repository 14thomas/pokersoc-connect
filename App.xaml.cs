using System;
using System.IO;
using System.Windows;

namespace pokersoc_connect
{
  public partial class App : Application
  {
    public App()
    {
      // Catch anything that would otherwise silently close the app
      this.DispatcherUnhandledException += (s, e) =>
      {
        MessageBox.Show(
          $"Unhandled error:\n{e.Exception}",
          "pokersoc-connect",
          MessageBoxButton.OK,
          MessageBoxImage.Error);
        e.Handled = true;
      };
    }

    protected override void OnStartup(StartupEventArgs e)
    {
      // IMPORTANT: ensure we don't shut down when StartWindow closes
      this.ShutdownMode = ShutdownMode.OnExplicitShutdown;

      base.OnStartup(e);

      var start = new StartWindow();
      bool? ok = start.ShowDialog();

      if (ok != true || string.IsNullOrWhiteSpace(start.SelectedPath))
      {
        Shutdown();
        return;
      }

      string dbPath = start.SelectedPath;

      // Ensure folder exists for new files (SaveFileDialog can point to a new folder)
      try
      {
        var dir = Path.GetDirectoryName(dbPath);
        if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
          Directory.CreateDirectory(dir);
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Could not create folder for database:\n{ex.Message}", "pokersoc-connect",
          MessageBoxButton.OK, MessageBoxImage.Error);
        Shutdown();
        return;
      }

      try
      {
        Database.Open(dbPath); // creates and applies schema if new
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Could not open database:\n{ex.Message}", "pokersoc-connect",
          MessageBoxButton.OK, MessageBoxImage.Error);
        Shutdown();
        return;
      }

      // Import players if requested
      if (!string.IsNullOrWhiteSpace(start.PlayerImportPath) && File.Exists(start.PlayerImportPath))
      {
        try
        {
          Database.ImportPlayers(start.PlayerImportPath, merge: true);
          MessageBox.Show($"Players imported successfully from:\n{start.PlayerImportPath}", 
            "Import Complete", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception ex)
        {
          MessageBox.Show($"Could not import players:\n{ex.Message}\n\nContinuing without import.", 
            "Import Warning", MessageBoxButton.OK, MessageBoxImage.Warning);
        }
      }

      // Now create and show the real main window
      var main = new MainWindow();
      this.MainWindow = main;
      main.Show();

      // Normal behavior from here on
      this.ShutdownMode = ShutdownMode.OnMainWindowClose;
    }
  }
}
