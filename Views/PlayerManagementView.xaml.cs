using System;
using System.Data;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using Microsoft.Win32;

namespace pokersoc_connect.Views
{
  public partial class PlayerManagementView : UserControl
  {
    public event EventHandler? CloseRequested;
    private string _selectedPlayerId = string.Empty;
    private string _enteredPassword = string.Empty;
    private bool _pendingSetUnderage = false; // true = set underage, false = set 18+

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
          SELECT player_id, display_name, 
                 CASE WHEN COALESCE(is_underage, 0) = 1 THEN 'Underage' ELSE '18+' END as age_status,
                 email, student_number, degree, study_year, arc_member, created_at 
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

    private void AgeVerifyIdBox_KeyDown(object sender, KeyEventArgs e)
    {
      if (e.Key == Key.Enter)
      {
        var playerId = AgeVerifyIdBox.Text.Trim();
        if (!string.IsNullOrWhiteSpace(playerId))
        {
          // Check if player exists
          if (Database.IsNewPlayer(playerId))
          {
            MessageBox.Show($"Player '{playerId}' not found in database.", "Player Not Found", 
              MessageBoxButton.OK, MessageBoxImage.Warning);
            SetAdultBtn.IsEnabled = false;
            SetUnderageBtn.IsEnabled = false;
          }
          else
          {
            _selectedPlayerId = playerId;
            SetAdultBtn.IsEnabled = true;
            SetUnderageBtn.IsEnabled = true;
          }
        }
        e.Handled = true;
      }
    }

    private void PlayersGrid_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      if (PlayersGrid.SelectedItem is DataRowView row)
      {
        _selectedPlayerId = row["player_id"]?.ToString() ?? string.Empty;
        AgeVerifyIdBox.Text = _selectedPlayerId;
        SetAdultBtn.IsEnabled = !string.IsNullOrWhiteSpace(_selectedPlayerId);
        SetUnderageBtn.IsEnabled = !string.IsNullOrWhiteSpace(_selectedPlayerId);
      }
    }

    private void SetAdult_Click(object sender, RoutedEventArgs e)
    {
      if (string.IsNullOrWhiteSpace(_selectedPlayerId))
      {
        MessageBox.Show("Please scan or select a player first.", "No Player Selected", 
          MessageBoxButton.OK, MessageBoxImage.Warning);
        return;
      }

      // Show password overlay
      _pendingSetUnderage = false;
      PasswordActionText.Text = $"to set '{_selectedPlayerId}' as 18+";
      ShowPasswordOverlay();
    }

    private void SetUnderage_Click(object sender, RoutedEventArgs e)
    {
      if (string.IsNullOrWhiteSpace(_selectedPlayerId))
      {
        MessageBox.Show("Please scan or select a player first.", "No Player Selected", 
          MessageBoxButton.OK, MessageBoxImage.Warning);
        return;
      }

      // Show password overlay
      _pendingSetUnderage = true;
      PasswordActionText.Text = $"to set '{_selectedPlayerId}' as Underage";
      ShowPasswordOverlay();
    }

    private void ShowPasswordOverlay()
    {
      _enteredPassword = string.Empty;
      PasswordDisplay.Text = string.Empty;
      PasswordErrorText.Visibility = Visibility.Collapsed;
      PasswordOverlay.Visibility = Visibility.Visible;
    }

    private void HidePasswordOverlay()
    {
      PasswordOverlay.Visibility = Visibility.Collapsed;
      _enteredPassword = string.Empty;
      PasswordDisplay.Text = string.Empty;
    }

    private void PasswordKeypad_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button btn && btn.Tag is string digit)
      {
        _enteredPassword += digit;
        PasswordDisplay.Text = new string('●', _enteredPassword.Length);
      }
    }

    private void PasswordBackspace_Click(object sender, RoutedEventArgs e)
    {
      if (_enteredPassword.Length > 0)
      {
        _enteredPassword = _enteredPassword.Substring(0, _enteredPassword.Length - 1);
        PasswordDisplay.Text = new string('●', _enteredPassword.Length);
      }
    }

    private void PasswordClear_Click(object sender, RoutedEventArgs e)
    {
      _enteredPassword = string.Empty;
      PasswordDisplay.Text = string.Empty;
    }

    private void PasswordCancel_Click(object sender, RoutedEventArgs e)
    {
      HidePasswordOverlay();
    }

    private void PasswordConfirm_Click(object sender, RoutedEventArgs e)
    {
      var adminPassword = Database.GetAdminPassword();
      if (_enteredPassword != adminPassword)
      {
        PasswordErrorText.Visibility = Visibility.Visible;
        _enteredPassword = string.Empty;
        PasswordDisplay.Text = string.Empty;
        return;
      }

      HidePasswordOverlay();

      try
      {
        Database.SetPlayerUnderage(_selectedPlayerId, _pendingSetUnderage);
        var statusText = _pendingSetUnderage ? "Underage" : "18+";
        ShowNotification("✓", "Age Updated", $"Player '{_selectedPlayerId}' marked as {statusText}.", true);
        LoadPlayers();
        AgeVerifyIdBox.Text = string.Empty;
        _selectedPlayerId = string.Empty;
        SetAdultBtn.IsEnabled = false;
        SetUnderageBtn.IsEnabled = false;
      }
      catch (Exception ex)
      {
        ShowNotification("✕", "Error", $"Error updating player age: {ex.Message}", false);
      }
    }

    private void ShowNotification(string icon, string title, string message, bool isSuccess)
    {
      NotificationIcon.Text = icon;
      NotificationIcon.Foreground = isSuccess 
        ? System.Windows.Media.Brushes.Green 
        : System.Windows.Media.Brushes.Red;
      NotificationTitle.Text = title;
      NotificationMessage.Text = message;
      NotificationOverlay.Visibility = Visibility.Visible;
    }

    private void NotificationOK_Click(object sender, RoutedEventArgs e)
    {
      NotificationOverlay.Visibility = Visibility.Collapsed;
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

    private void Close_Click(object sender, RoutedEventArgs e)
    {
      CloseRequested?.Invoke(this, EventArgs.Empty);
    }
  }
}
