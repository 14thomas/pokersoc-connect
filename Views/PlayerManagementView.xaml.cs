using System;
using System.Data;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Win32;

namespace pokersoc_connect.Views
{
  public partial class PlayerManagementView : UserControl
  {
    public event EventHandler? CloseRequested;

    public PlayerManagementView()
    {
      InitializeComponent();
      Loaded += (s, e) => LoadPlayers();
    }

    private void LoadPlayers()
    {
      try
      {
        var players = Database.Query(@"
          SELECT player_id, display_name, first_name, last_name, phone, notes, status, created_at 
          FROM players 
          ORDER BY created_at DESC");
        PlayersGrid.ItemsSource = players.DefaultView;
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error loading players: {ex.Message}", "Error", 
          MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void Import_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        var dialog = new OpenFileDialog
        {
          Title = "Import Player Database",
          Filter = "CSV Files (*.csv)|*.csv|All Files (*.*)|*.*",
          CheckFileExists = true
        };

        if (dialog.ShowDialog() == true)
        {
          var result = MessageBox.Show(
            "How would you like to import?\n\n" +
            "YES = Merge (update existing, add new)\n" +
            "NO = Add only new players\n" +
            "CANCEL = Cancel import",
            "Import Mode",
            MessageBoxButton.YesNoCancel,
            MessageBoxImage.Question);

          if (result == MessageBoxResult.Cancel) return;

          bool merge = result == MessageBoxResult.Yes;
          Database.ImportPlayers(dialog.FileName, merge);

          MessageBox.Show("Players imported successfully!", "Success", 
            MessageBoxButton.OK, MessageBoxImage.Information);
          LoadPlayers();
        }
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error importing players: {ex.Message}", "Error", 
          MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void Export_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        var dialog = new SaveFileDialog
        {
          Title = "Export Player Database",
          Filter = "CSV Files (*.csv)|*.csv|All Files (*.*)|*.*",
          FileName = $"players_{DateTime.Now:yyyy-MM-dd}.csv"
        };

        if (dialog.ShowDialog() == true)
        {
          Database.ExportPlayers(dialog.FileName);
          MessageBox.Show($"Players exported successfully to:\n{dialog.FileName}", 
            "Success", MessageBoxButton.OK, MessageBoxImage.Information);
        }
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error exporting players: {ex.Message}", "Error", 
          MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void Refresh_Click(object sender, RoutedEventArgs e)
    {
      LoadPlayers();
    }

    private void Close_Click(object sender, RoutedEventArgs e)
    {
      CloseRequested?.Invoke(this, EventArgs.Empty);
    }
  }
}

