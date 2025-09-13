using System;
using System.IO;
using System.Reflection;
using System.Windows;

namespace pokersoc_connect
{
  public partial class App : Application
  {
    private void OnStartup(object sender, StartupEventArgs e)
    {
      // Global exception surface so crashes show a dialog (not silent exits)
      AppDomain.CurrentDomain.UnhandledException += (s, ex) =>
        MessageBox.Show(ex.ExceptionObject.ToString(), "Unhandled Exception");

      var start = new StartWindow();
      var ok = start.ShowDialog() == true;
      if (!ok || string.IsNullOrWhiteSpace(start.SelectedPath))
      {
        Shutdown();
        return;
      }

      try
      {
        Directory.CreateDirectory(Path.GetDirectoryName(start.SelectedPath)!);

        // Open DB and apply schema from embedded resource
        Database.Open(start.SelectedPath);
        Database.EnsureSchemaFromResource(Assembly.GetExecutingAssembly(), "pokersoc_connect.schema.sql");

        // Seed one session if fresh
        Database.SeedDefaultSessionIfEmpty();

        var mw = new MainWindow();
        mw.Title = $"pokersoc-connect — {Path.GetFileName(start.SelectedPath)}";
        MainWindow = mw;
        mw.Show();
      }
      catch (Exception ex)
      {
        MessageBox.Show(ex.Message, "Startup Error");
        Shutdown(1);
      }
    }
  }
}
