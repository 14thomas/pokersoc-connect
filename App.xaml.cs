using System;
using System.IO;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;

namespace pokersoc_connect
{
  public partial class App : Application
  {
    private static string LogPath => Path.Combine(AppContext.BaseDirectory, "pokersoc_connect.log");
    private static void Log(string msg)
    {
      try { File.AppendAllText(LogPath, $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] {msg}\r\n"); } catch {}
    }

    private void OnStartup(object sender, StartupEventArgs e)
    {
      // Surface any exceptions instead of silent exits
      AppDomain.CurrentDomain.UnhandledException += (s, ex) =>
      {
        var text = ex.ExceptionObject?.ToString() ?? "Unknown unhandled exception.";
        Log("AppDomain.UnhandledException: " + text);
        MessageBox.Show(text, "Unhandled Exception");
      };
      DispatcherUnhandledException += (s, ex) =>
      {
        Log("DispatcherUnhandledException: " + ex.Exception);
        MessageBox.Show(ex.Exception.ToString(), "Unhandled UI Exception");
        ex.Handled = true;
      };
      TaskScheduler.UnobservedTaskException += (s, ex) =>
      {
        Log("TaskScheduler.UnobservedTaskException: " + ex.Exception);
        MessageBox.Show(ex.Exception.ToString(), "Unhandled Task Exception");
        ex.SetObserved();
      };

      try
      {
        // IMPORTANT: don't auto-shutdown when StartWindow closes
        ShutdownMode = ShutdownMode.OnExplicitShutdown;

        var start = new StartWindow();
        var ok = start.ShowDialog() == true;
        Log($"StartWindow: ok={ok}, path='{start.SelectedPath}'");

        if (!ok || string.IsNullOrWhiteSpace(start.SelectedPath))
        {
          Log("No file chosen. Shutting down.");
          Shutdown();
          return;
        }

        var path = start.SelectedPath!;
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);

        // Open DB and apply schema
        Database.Open(path);

        var asm = Assembly.GetExecutingAssembly();
        var resName = Database.FindResourceName(asm, "schema.sql")
                     ?? throw new InvalidOperationException("Embedded resource 'schema.sql' not found.");
        Database.EnsureSchemaFromResource(asm, resName);
        Database.SeedDefaultSessionIfEmpty();

        // Show main window, then hand shutdown control back to WPF
        var mw = new MainWindow
        {
          Title = $"pokersoc-connect — {Path.GetFileName(path)}"
        };
        MainWindow = mw;
        mw.Show();

        ShutdownMode = ShutdownMode.OnMainWindowClose;
        Log("Startup complete.");
      }
      catch (Exception ex)
      {
        Log("Startup Error: " + ex);
        MessageBox.Show(ex.ToString(), "Startup Error");
        Shutdown(1);
      }
    }
  }
}
