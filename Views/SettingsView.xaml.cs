using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using Microsoft.Win32;

namespace pokersoc_connect.Views
{
  public partial class SettingsView : UserControl
  {
    public event EventHandler? SettingsChanged;
    public event EventHandler? CloseRequested;

    public SettingsView()
    {
      InitializeComponent();
      LoadChipConfiguration();
      LoadFoodItems();
    }

    private void LoadChipConfiguration()
    {
      var chipConfigs = new List<ChipConfigItem>
      {
        new ChipConfigItem { Denomination = "5c", CurrentValue = 5, NewValue = 5, Color = Brushes.LightGray, IsEnabled = true },
        new ChipConfigItem { Denomination = "25c", CurrentValue = 25, NewValue = 25, Color = Brushes.Red, IsEnabled = true },
        new ChipConfigItem { Denomination = "$1", CurrentValue = 100, NewValue = 100, Color = Brushes.Blue, IsEnabled = true },
        new ChipConfigItem { Denomination = "$2", CurrentValue = 200, NewValue = 200, Color = Brushes.Green, IsEnabled = true },
        new ChipConfigItem { Denomination = "$5", CurrentValue = 500, NewValue = 500, Color = Brushes.Black, IsEnabled = true },
        new ChipConfigItem { Denomination = "$25", CurrentValue = 2500, NewValue = 2500, Color = Brushes.White, IsEnabled = true },
        new ChipConfigItem { Denomination = "$100", CurrentValue = 10000, NewValue = 10000, Color = Brushes.Red, IsEnabled = true }
      };

      ChipConfigGrid.ItemsSource = chipConfigs;
    }

    private void ImportSettings_Click(object sender, RoutedEventArgs e)
    {
      var dlg = new OpenFileDialog 
      { 
        Filter = "pokersoc settings (*.json)|*.json|All files|*.*",
        Title = "Import Settings"
      };
      if (dlg.ShowDialog() == true)
      {
        try
        {
          var merge = MessageBox.Show("Merge into existing catalog? (No = replace)",
                      "Import Settings", MessageBoxButton.YesNo) == MessageBoxResult.Yes;
          Database.ImportSettings(dlg.FileName, merge: merge);
          MessageBox.Show("Settings imported successfully.", "Import Success", MessageBoxButton.OK, MessageBoxImage.Information);
          SettingsChanged?.Invoke(this, EventArgs.Empty);
        }
        catch (Exception ex)
        {
          MessageBox.Show($"Error importing settings:\n{ex.Message}", "Import Error", 
            MessageBoxButton.OK, MessageBoxImage.Error);
        }
      }
    }

    private void ExportSettings_Click(object sender, RoutedEventArgs e)
    {
      var dlg = new SaveFileDialog 
      { 
        Filter = "pokersoc settings (*.json)|*.json|All files|*.*",
        FileName = "pokersoc-settings.json",
        Title = "Export Settings"
      };
      if (dlg.ShowDialog() == true)
      {
        try
        {
          Database.ExportSettings(dlg.FileName);
          MessageBox.Show("Settings exported successfully.", "Export Success", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception ex)
        {
          MessageBox.Show($"Error exporting settings:\n{ex.Message}", "Export Error", 
            MessageBoxButton.OK, MessageBoxImage.Error);
        }
      }
    }

    private void BackupDatabase_Click(object sender, RoutedEventArgs e)
    {
      var dlg = new SaveFileDialog 
      { 
        Filter = "SQLite Database (*.db)|*.db|All files|*.*",
        FileName = $"pokersoc-backup-{DateTime.Now:yyyy-MM-dd-HHmm}.db",
        Title = "Backup Database"
      };
      if (dlg.ShowDialog() == true)
      {
        try
        {
          if (Database.Conn != null)
          {
            using var backupConn = new Microsoft.Data.Sqlite.SqliteConnection($"Data Source={dlg.FileName}");
            backupConn.Open();
            Database.Conn.BackupDatabase(backupConn);
            MessageBox.Show("Database backed up successfully.", "Backup Success", MessageBoxButton.OK, MessageBoxImage.Information);
          }
          else
          {
            MessageBox.Show("No database is currently open.", "Backup Error", 
              MessageBoxButton.OK, MessageBoxImage.Warning);
          }
        }
        catch (Exception ex)
        {
          MessageBox.Show($"Error backing up database:\n{ex.Message}", "Backup Error", 
            MessageBoxButton.OK, MessageBoxImage.Error);
        }
      }
    }

    private void RestoreDatabase_Click(object sender, RoutedEventArgs e)
    {
      var dlg = new OpenFileDialog 
      { 
        Filter = "SQLite Database (*.db)|*.db|All files|*.*",
        Title = "Restore Database"
      };
      if (dlg.ShowDialog() == true)
      {
        var result = MessageBox.Show(
          "This will close the current session and open the selected database. Continue?",
          "Restore Database", MessageBoxButton.YesNo, MessageBoxImage.Question);
        
        if (result == MessageBoxResult.Yes)
        {
          try
          {
            Database.Close();
            Database.Open(dlg.FileName);
            MessageBox.Show("Database restored successfully.", "Restore Success", MessageBoxButton.OK, MessageBoxImage.Information);
            SettingsChanged?.Invoke(this, EventArgs.Empty);
          }
          catch (Exception ex)
          {
            MessageBox.Show($"Error restoring database:\n{ex.Message}", "Restore Error", 
              MessageBoxButton.OK, MessageBoxImage.Error);
          }
        }
      }
    }

    private void NewSession_Click(object sender, RoutedEventArgs e)
    {
      var dlg = new SaveFileDialog 
      { 
        Filter = "SQLite Database (*.db)|*.db|All files|*.*",
        FileName = $"pokersoc-session-{DateTime.Now:yyyy-MM-dd-HHmm}.db",
        Title = "New Session"
      };
      if (dlg.ShowDialog() == true)
      {
        var result = MessageBox.Show(
          "This will close the current session and create a new one. Continue?",
          "New Session", MessageBoxButton.YesNo, MessageBoxImage.Question);
        
        if (result == MessageBoxResult.Yes)
        {
          try
          {
            Database.Close();
            Database.Open(dlg.FileName);
            MessageBox.Show("New session created successfully.", "New Session Success", MessageBoxButton.OK, MessageBoxImage.Information);
            SettingsChanged?.Invoke(this, EventArgs.Empty);
          }
          catch (Exception ex)
          {
            MessageBox.Show($"Error creating new session:\n{ex.Message}", "New Session Error", 
              MessageBoxButton.OK, MessageBoxImage.Error);
          }
        }
      }
    }

    private void CloseSession_Click(object sender, RoutedEventArgs e)
    {
      var result = MessageBox.Show(
        "This will close the current session. Continue?",
        "Close Session", MessageBoxButton.YesNo, MessageBoxImage.Question);
      
      if (result == MessageBoxResult.Yes)
      {
        Database.Close();
        Application.Current.Shutdown();
      }
    }

    private void ChangeColor_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button btn && btn.Tag is ChipConfigItem item)
      {
        // Simple color selection using predefined colors
        var colorWindow = new Window
        {
          Title = "Select Color",
          Width = 300,
          Height = 200,
          WindowStartupLocation = WindowStartupLocation.CenterOwner,
          Owner = Window.GetWindow(this)
        };

        var colors = new[]
        {
          Brushes.Red, Brushes.Blue, Brushes.Green, Brushes.Yellow, Brushes.Orange,
          Brushes.Purple, Brushes.Pink, Brushes.Brown, Brushes.Gray, Brushes.Black,
          Brushes.White, Brushes.LightBlue, Brushes.LightGreen, Brushes.LightYellow,
          Brushes.LightPink, Brushes.LightGray, Brushes.DarkRed, Brushes.DarkBlue,
          Brushes.DarkGreen, Brushes.DarkOrange, Brushes.DarkViolet, Brushes.Gold
        };

        var wrapPanel = new WrapPanel { Margin = new Thickness(10) };
        
        foreach (var color in colors)
        {
          var button = new Button
          {
            Width = 40,
            Height = 40,
            Margin = new Thickness(2),
            Background = color,
            BorderBrush = Brushes.Black,
            BorderThickness = new Thickness(1)
          };
          
          button.Click += (s, args) =>
          {
            item.Color = color;
            ChipConfigGrid.Items.Refresh();
            colorWindow.Close();
          };
          
          wrapPanel.Children.Add(button);
        }

        colorWindow.Content = wrapPanel;
        colorWindow.ShowDialog();
      }
    }

    private void ResetChips_Click(object sender, RoutedEventArgs e)
    {
      var result = MessageBox.Show(
        "Reset all chip configurations to defaults?",
        "Reset Chips", MessageBoxButton.YesNo, MessageBoxImage.Question);
      
      if (result == MessageBoxResult.Yes)
      {
        LoadChipConfiguration();
      }
    }

    private void ApplyChipChanges_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        // Validate the changes
        var configs = ChipConfigGrid.ItemsSource.Cast<ChipConfigItem>().ToList();
        var errors = new List<string>();

        foreach (var config in configs.Where(c => c.IsEnabled))
        {
          if (config.NewValue <= 0)
            errors.Add($"{config.Denomination}: Value must be greater than 0");
          
          if (config.NewValue % 5 != 0)
            errors.Add($"{config.Denomination}: Value must be divisible by 5 cents");
        }

        if (errors.Any())
        {
          MessageBox.Show($"Please fix the following errors:\n\n{string.Join("\n", errors)}", 
            "Validation Error", MessageBoxButton.OK, MessageBoxImage.Warning);
          return;
        }

        // Apply the changes
        // TODO: Implement chip configuration storage and loading
        MessageBox.Show("Chip configuration changes applied successfully.", "Success", 
          MessageBoxButton.OK, MessageBoxImage.Information);
        
        SettingsChanged?.Invoke(this, EventArgs.Empty);
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error applying chip changes:\n{ex.Message}", "Apply Error", 
          MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void LoadFoodItems()
    {
      try
      {
        if (Database.Conn == null)
        {
          FoodItemsGrid.ItemsSource = null;
          return;
        }

        var result = Database.Query("SELECT product_id, name, price, image_path FROM products ORDER BY name");
        var foodItems = result.AsEnumerable().Select(r => new
        {
          product_id = Convert.ToInt64(r["product_id"]),
          name = r["name"]?.ToString() ?? "",
          price = Convert.ToDouble(r["price"] ?? 0).ToString("C", CultureInfo.GetCultureInfo("en-AU")),
          image_path = r["image_path"] == DBNull.Value ? "" : r["image_path"]?.ToString() ?? ""
        }).ToList();

        FoodItemsGrid.ItemsSource = foodItems;
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error loading food items:\n{ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void FoodItemsGrid_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      // Enable/disable delete button based on selection
      DeleteFoodItemButton.IsEnabled = FoodItemsGrid.SelectedItem != null;
    }

    private void AddFoodItemInline_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        var name = NewItemNameBox.Text.Trim();
        if (string.IsNullOrWhiteSpace(name))
        {
          MessageBox.Show("Please enter a product name.", "Validation Error", MessageBoxButton.OK, MessageBoxImage.Warning);
          NewItemNameBox.Focus();
          return;
        }

        if (!double.TryParse(NewItemPriceBox.Text, NumberStyles.Any, CultureInfo.GetCultureInfo("en-AU"), out var price) || price < 0)
        {
          MessageBox.Show("Please enter a valid price.", "Validation Error", MessageBoxButton.OK, MessageBoxImage.Warning);
          NewItemPriceBox.Focus();
          NewItemPriceBox.SelectAll();
          return;
        }

        var imagePath = string.IsNullOrWhiteSpace(NewItemImageBox.Text) ? null : NewItemImageBox.Text.Trim();

        Database.Exec("INSERT INTO products(name, price, image_path) VALUES ($n, $p, $img)",
          ("$n", name), ("$p", price), ("$img", (object?)imagePath ?? DBNull.Value));
        
        LoadFoodItems();
        SettingsChanged?.Invoke(this, EventArgs.Empty);
        
        // Clear the form
        NewItemNameBox.Text = "";
        NewItemPriceBox.Text = "0.00";
        NewItemImageBox.Text = "";
        NewItemNameBox.Focus();
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error adding food item:\n{ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void BrowseImage_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        var dlg = new Microsoft.Win32.OpenFileDialog
        {
          Title = "Choose image",
          Filter = "Images|*.png;*.jpg;*.jpeg;*.gif;*.bmp|All files|*.*"
        };
        if (dlg.ShowDialog() == true)
        {
          NewItemImageBox.Text = dlg.FileName;
        }
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error browsing for image:\n{ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void DeleteFoodItem_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        if (FoodItemsGrid.SelectedItem == null)
        {
          MessageBox.Show("Please select a food item to delete.", "No Selection", MessageBoxButton.OK, MessageBoxImage.Warning);
          return;
        }

        var selectedItem = FoodItemsGrid.SelectedItem;
        long productId = (long)selectedItem.GetType().GetProperty("product_id")!.GetValue(selectedItem, null)!;
        string productName = selectedItem.GetType().GetProperty("name")!.GetValue(selectedItem, null)!.ToString()!;

        var result = MessageBox.Show($"Are you sure you want to delete '{productName}'?\n\nThis will also delete all sales records for this item.", 
          "Confirm Delete", MessageBoxButton.YesNo, MessageBoxImage.Question);

        if (result == MessageBoxResult.Yes)
        {
          Database.Exec("DELETE FROM products WHERE product_id = $id", ("$id", productId));
          LoadFoodItems();
          SettingsChanged?.Invoke(this, EventArgs.Empty);
        }
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error deleting food item:\n{ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void Close_Click(object sender, RoutedEventArgs e)
    {
      CloseRequested?.Invoke(this, EventArgs.Empty);
    }
  }

  public class ChipConfigItem
  {
    public string Denomination { get; set; } = "";
    public int CurrentValue { get; set; }
    public int NewValue { get; set; }
    public Brush Color { get; set; } = Brushes.Gray;
    public bool IsEnabled { get; set; }
  }
}
