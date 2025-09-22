using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace pokersoc_connect.Views
{
  public partial class FoodCatalogView : UserControl
  {
    public event EventHandler? BackToMain;
    public event EventHandler? GoToSettings;
    public event EventHandler? ActivityRefreshRequested;

    // Data classes
    private sealed class Product
    {
      public long Id { get; set; }
      public string Name { get; set; } = "";
      public double Price { get; set; }
      public string Icon { get; set; } = "🍕";
    }

    private sealed class ItemSlot
    {
      public Product? Product { get; set; }
      public Button Button { get; set; } = null!;
      public TextBlock NameText { get; set; } = null!;
      public TextBlock PriceText { get; set; } = null!;
      public TextBlock IconText { get; set; } = null!;
    }

    // Constants
    private static readonly int[] CashDenominations = { 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    private static readonly int[] ChipDenominations = { 5, 25, 100, 200, 500, 2500, 10000 };
    private static readonly CultureInfo AU = CultureInfo.GetCultureInfo("en-AU");
    
    // Payment button references
    private readonly Button[] _cashButtons = new Button[11];
    private readonly Button[] _chipButtons = new Button[7];
    private readonly TextBlock[] _cashCounts = new TextBlock[11];
    private readonly TextBlock[] _chipCounts = new TextBlock[7];

    // State
    private List<Product> _products = new();
    private List<ItemSlot> _itemSlots = new();
    private Product? _selectedProductForPayment;
    private Dictionary<int, int> _cashDenominations = new(); // denomination -> quantity
    private Dictionary<int, int> _chipDenominations = new(); // denomination -> quantity
    private int _selectedSlotIndex = -1; // for product selection
    private bool _isCashMode = false; // true for cash, false for chips (default to chips)
    private int _chipMultiplier = 1;

    public FoodCatalogView()
    {
      InitializeComponent();
      Loaded += (s, e) => InitializeView();
    }

    private void InitializeView()
    {
      try
      {
        LoadProducts();
        LoadItemSlots();
        LoadSlotAssignments();
        LoadPaymentButtons();
        UpdateStats();
        UpdateTotalMoneyDisplay();
      }
      catch (Exception ex)
      {
        // Silently handle initialization errors
      }
    }

    private void LoadPaymentButtons()
    {
      try
      {
        // Cash buttons
        _cashButtons[0] = Cash5c; _cashCounts[0] = Cash5cCount;
        _cashButtons[1] = Cash10c; _cashCounts[1] = Cash10cCount;
        _cashButtons[2] = Cash20c; _cashCounts[2] = Cash20cCount;
        _cashButtons[3] = Cash50c; _cashCounts[3] = Cash50cCount;
        _cashButtons[4] = Cash1; _cashCounts[4] = Cash1Count;
        _cashButtons[5] = Cash2; _cashCounts[5] = Cash2Count;
        _cashButtons[6] = Cash5; _cashCounts[6] = Cash5Count;
        _cashButtons[7] = Cash10; _cashCounts[7] = Cash10Count;
        _cashButtons[8] = Cash20; _cashCounts[8] = Cash20Count;
        _cashButtons[9] = Cash50; _cashCounts[9] = Cash50Count;
        _cashButtons[10] = Cash100; _cashCounts[10] = Cash100Count;

        // Chip buttons
        _chipButtons[0] = Chip5c; _chipCounts[0] = Chip5cCount; // 5c
        _chipButtons[1] = Chip25c; _chipCounts[1] = Chip25cCount; // 25c
        _chipButtons[2] = Chip1; _chipCounts[2] = Chip1Count; // $1
        _chipButtons[3] = Chip2; _chipCounts[3] = Chip2Count; // $2
        _chipButtons[4] = Chip5; _chipCounts[4] = Chip5Count; // $5
        _chipButtons[5] = Chip25; _chipCounts[5] = Chip25Count; // $25
        _chipButtons[6] = Chip100; _chipCounts[6] = Chip100Count; // $100
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void LoadProducts()
    {
      try
      {
        if (Database.Conn == null)
        {
          _products.Clear();
          return;
        }

        // Check if products table exists
        var tableCheck = Database.Query("SELECT name FROM sqlite_master WHERE type='table' AND name='products'");
        if (tableCheck == null || tableCheck.Rows.Count == 0)
        {
          _products.Clear();
          return;
        }

        var query = "SELECT product_id, name, price FROM products ORDER BY name";
        var result = Database.Query(query);
        
        if (result == null)
        {
          _products.Clear();
          return;
        }

        _products = result.Rows.Cast<DataRow>()
          .Select(row => new Product
          {
            Id = Convert.ToInt64(row["product_id"]),
            Name = row["name"]?.ToString() ?? "",
            Price = Convert.ToDouble(row["price"] ?? 0)
          })
          .ToList();
      }
      catch (Exception ex)
      {
        // Silently handle errors
        _products.Clear();
      }
    }

    private void LoadItemSlots()
    {
      try
      {
        _itemSlots.Clear();
        
        // Get all item buttons and their text blocks (excluding TotalMoneyButton which is now button 6)
        var buttons = new[] { ItemButton1, ItemButton2, ItemButton3, ItemButton4, ItemButton5 };
        var nameTexts = new[] { Item1Name, Item2Name, Item3Name, Item4Name, Item5Name };
        var priceTexts = new[] { Item1Price, Item2Price, Item3Price, Item4Price, Item5Price };
        var iconTexts = new[] { Item1Icon, Item2Icon, Item3Icon, Item4Icon, Item5Icon };

        for (int i = 0; i < buttons.Length; i++)
        {
          _itemSlots.Add(new ItemSlot
          {
            Product = null,
            Button = buttons[i],
            NameText = nameTexts[i],
            PriceText = priceTexts[i],
            IconText = iconTexts[i]
          });
        }
        
        UpdateStats();
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void LoadSlotAssignments()
    {
      try
      {
        if (Database.Conn == null) return;
        
        var assignments = Database.LoadFoodSlotAssignments();
        
        foreach (var assignment in assignments)
        {
          var slotIndex = assignment.Key - 1; // Convert 1-based slot ID to 0-based index
          var productId = assignment.Value;
          
          if (slotIndex >= 0 && slotIndex < _itemSlots.Count)
          {
            var product = _products.FirstOrDefault(p => p.Id == productId);
            if (product != null)
            {
              AssignProductToSlot(slotIndex, product);
            }
          }
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void UpdateStats()
    {
      try
      {
        if (TotalProductsText == null || FilledSlotsText == null) return;
        
        var totalProducts = _products.Count;
        var filledSlots = _itemSlots.Count(slot => slot.Product != null);
        
        TotalProductsText.Text = $"Products: {totalProducts}";
        FilledSlotsText.Text = $"Filled Slots: {filledSlots}/6";
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }


    // Event Handlers
    private void ItemButton_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        if (sender is Button button && int.TryParse(button.Tag?.ToString(), out int slotIndex))
        {
          var slot = _itemSlots[slotIndex - 1];
          
          if (slot.Product == null)
          {
            // Show product selection dialog
            ShowProductSelectionDialog(slotIndex - 1);
          }
          else
          {
            // Go to payment screen
            ShowPaymentScreen(slot.Product);
          }
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void ShowProductSelectionDialog(int slotIndex)
    {
      try
      {
        if (_products.Count == 0)
        {
          // No products available - just return silently for fullscreen mode
          return;
        }

        _selectedSlotIndex = slotIndex;
        LoadProductSelectionPanel();
        
        // Show product selection screen
        MainFoodView.Visibility = Visibility.Collapsed;
        ProductSelectionView.Visibility = Visibility.Visible;
      }
      catch (Exception ex)
      {
        // Silently handle errors for fullscreen mode
      }
    }

    private void LoadProductSelectionPanel()
    {
      try
      {
        if (ProductSelectionPanel == null) return;
        
        ProductSelectionPanel.Children.Clear();
        
        foreach (var product in _products)
        {
          var button = new Button
          {
            Content = $"{product.Icon} {product.Name} - {product.Price:C}",
            Margin = new Thickness(0, 0, 0, 10),
            Padding = new Thickness(15, 15, 15, 15),
            FontSize = 16,
            FontWeight = FontWeights.Bold,
            Tag = product,
            Height = 60
          };
          
          button.Click += ProductSelectionButton_Click;
          ProductSelectionPanel.Children.Add(button);
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void ProductSelectionButton_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        if (sender is Button button && button.Tag is Product product && _selectedSlotIndex >= 0)
        {
          AssignProductToSlot(_selectedSlotIndex, product);
          BackToFoodFromSelection_Click(sender, e);
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void BackToFoodFromSelection_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        ProductSelectionView.Visibility = Visibility.Collapsed;
        MainFoodView.Visibility = Visibility.Visible;
        _selectedSlotIndex = -1;
        UpdateTotalMoneyDisplay();
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void AssignProductToSlot(int slotIndex, Product product)
    {
      try
      {
        var slot = _itemSlots[slotIndex];
        slot.Product = product;
        
        slot.NameText.Text = product.Name;
        slot.PriceText.Text = product.Price.ToString("C", AU);
        slot.IconText.Text = product.Icon;
        
        // Update button appearance
        slot.Button.Background = Brushes.LightGreen;
        slot.Button.BorderBrush = Brushes.Green;
        
        // Save to database (slot ID is 1-based)
        Database.SaveFoodSlotAssignment(slotIndex + 1, (int)product.Id);
        
        UpdateStats();
      }
      catch (Exception ex)
      {
        // Silently handle errors for fullscreen mode
      }
    }

    private void ClearSlotAssignment(int slotIndex)
    {
      try
      {
        var slot = _itemSlots[slotIndex];
        slot.Product = null;
        
        slot.NameText.Text = "Empty Slot";
        slot.PriceText.Text = "Click to assign";
        slot.IconText.Text = "🍕";
        
        // Reset button appearance
        slot.Button.Background = Brushes.White;
        slot.Button.BorderBrush = Brushes.LightGray;
        
        // Remove from database (slot ID is 1-based)
        Database.SaveFoodSlotAssignment(slotIndex + 1, null);
        
        UpdateStats();
      }
      catch (Exception ex)
      {
        // Silently handle errors for fullscreen mode
      }
    }

    private void ShowPaymentScreen(Product product)
    {
      try
      {
        _selectedProductForPayment = product;
        _cashDenominations.Clear();
        _chipDenominations.Clear();
        
        // Update payment screen
        PaymentProductName.Text = product.Name;
        PaymentProductPrice.Text = product.Price.ToString("C", AU);
        PaymentPlayerIdTextBox.Text = "";
        
        // Show payment screen
        MainFoodView.Visibility = Visibility.Collapsed;
        PaymentView.Visibility = Visibility.Visible;
        
        // Initialize payment buttons
        ClearAllPayments();
        UpdatePaymentMethodToggle();
        UpdatePaymentSummary();
      }
      catch (Exception ex)
      {
        // Silently handle errors for fullscreen mode
      }
    }

    private void UpdatePaymentMethodToggle()
    {
      try
      {
        if (_isCashMode)
        {
          PaymentMethodIcon.Text = "💰";
          PaymentMethodText.Text = "Cash";
          CashPaymentView.Visibility = Visibility.Visible;
          ChipPaymentView.Visibility = Visibility.Collapsed;
        }
        else
        {
          PaymentMethodIcon.Text = "🪙";
          PaymentMethodText.Text = "Chips";
          CashPaymentView.Visibility = Visibility.Collapsed;
          ChipPaymentView.Visibility = Visibility.Visible;
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void CashDenom_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        if (sender is Button button && int.TryParse(button.Tag?.ToString(), out int denomination))
        {
          // Find the count text block for this button
          TextBlock? countText = null;
          
          if (button == Cash5c) countText = Cash5cCount;
          else if (button == Cash10c) countText = Cash10cCount;
          else if (button == Cash20c) countText = Cash20cCount;
          else if (button == Cash50c) countText = Cash50cCount;
          else if (button == Cash1) countText = Cash1Count;
          else if (button == Cash2) countText = Cash2Count;
          else if (button == Cash5) countText = Cash5Count;
          else if (button == Cash10) countText = Cash10Count;
          else if (button == Cash20) countText = Cash20Count;
          else if (button == Cash50) countText = Cash50Count;
          else if (button == Cash100) countText = Cash100Count;
          
          if (countText != null)
          {
            var currentCount = int.Parse(countText.Text);
            countText.Text = (currentCount + 1).ToString();
            
            if (_cashDenominations.ContainsKey(denomination))
            {
              _cashDenominations[denomination]++;
            }
            else
            {
              _cashDenominations[denomination] = 1;
            }
            
            UpdatePaymentSummary();
          }
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void ChipDenom_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        if (sender is Button button && int.TryParse(button.Tag?.ToString(), out int denomination))
        {
          // Find the count text block for this button
          TextBlock? countText = null;
          
          if (button == Chip5c) countText = Chip5cCount;
          else if (button == Chip25c) countText = Chip25cCount;
          else if (button == Chip1) countText = Chip1Count;
          else if (button == Chip2) countText = Chip2Count;
          else if (button == Chip5) countText = Chip5Count;
          else if (button == Chip25) countText = Chip25Count;
          else if (button == Chip100) countText = Chip100Count;
          
          if (countText != null)
          {
            var currentCount = int.Parse(countText.Text);
            countText.Text = (currentCount + _chipMultiplier).ToString();
            
            if (_chipDenominations.ContainsKey(denomination))
            {
              _chipDenominations[denomination] += _chipMultiplier;
            }
            else
            {
              _chipDenominations[denomination] = _chipMultiplier;
            }
            
            UpdatePaymentSummary();
          }
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void CashClear_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        _cashDenominations.Clear();
        
        // Reset all cash count displays
        Cash5cCount.Text = "0";
        Cash10cCount.Text = "0";
        Cash20cCount.Text = "0";
        Cash50cCount.Text = "0";
        Cash1Count.Text = "0";
        Cash2Count.Text = "0";
        Cash5Count.Text = "0";
        Cash10Count.Text = "0";
        Cash20Count.Text = "0";
        Cash50Count.Text = "0";
        Cash100Count.Text = "0";
        
        UpdatePaymentSummary();
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void ClearAllPayments()
    {
      try
      {
        _cashDenominations.Clear();
        _chipDenominations.Clear();
        
        // Reset all count displays
        Cash5cCount.Text = "0";
        Cash10cCount.Text = "0";
        Cash20cCount.Text = "0";
        Cash50cCount.Text = "0";
        Cash1Count.Text = "0";
        Cash2Count.Text = "0";
        Cash5Count.Text = "0";
        Cash10Count.Text = "0";
        Cash20Count.Text = "0";
        Cash50Count.Text = "0";
        Cash100Count.Text = "0";
        
        Chip5cCount.Text = "0";
        Chip25cCount.Text = "0";
        Chip1Count.Text = "0";
        Chip2Count.Text = "0";
        Chip5Count.Text = "0";
        Chip25Count.Text = "0";
        Chip100Count.Text = "0";
        
        UpdatePaymentSummary();
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void UpdatePaymentSummary()
    {
      try
      {
        if (_selectedProductForPayment == null || PaymentSummaryText == null || PaymentChangeText == null)
          return;
        
        var productTotal = _selectedProductForPayment.Price;
        var paymentTotal = 0.0;
        var currentDenominations = _isCashMode ? _cashDenominations : _chipDenominations;
        
        paymentTotal = currentDenominations.Sum(kvp => (kvp.Key * kvp.Value) / 100.0);
        var change = paymentTotal - productTotal;
        
        PaymentSummaryText.Text = $"Product: {productTotal.ToString("C", AU)} | Payment: {paymentTotal.ToString("C", AU)}";
        
        if (change >= 0)
        {
          PaymentChangeText.Text = $"Change: {change.ToString("C", AU)}";
          PaymentChangeText.Foreground = Brushes.Green;
          
          // Enable appropriate confirm button
          if (ConfirmPaymentButton != null)
          {
            ConfirmPaymentButton.IsEnabled = true;
            ConfirmPaymentButton.Opacity = 1.0;
          }
          
          // Show change breakdown for cash payments
          if (_isCashMode && change > 0)
          {
            ShowChangeBreakdown(change);
          }
          else
          {
            HideChangeBreakdown();
          }
        }
        else
        {
          PaymentChangeText.Text = $"Owing: {Math.Abs(change).ToString("C", AU)}";
          PaymentChangeText.Foreground = Brushes.Red;
          
          // Disable appropriate confirm button
          if (_isCashMode && ConfirmPaymentButton != null)
          {
            ConfirmPaymentButton.IsEnabled = false;
            ConfirmPaymentButton.Opacity = 0.6;
          }
          else if (ConfirmPaymentButton != null)
          {
            ConfirmPaymentButton.IsEnabled = false;
            ConfirmPaymentButton.Opacity = 0.6;
          }
          
          HideChangeBreakdown();
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private string FormatDenomination(int cents)
    {
      if (cents >= 100)
        return $"${cents / 100}";
      else
        return $"{cents}c";
    }

    private void PaymentMethodToggle_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        _isCashMode = !_isCashMode;
        ClearAllPayments();
        UpdatePaymentMethodToggle();
        UpdatePaymentSummary();
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void ChipMultiplier_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        // Cycle through 1x, 5x, 20x
        _chipMultiplier = _chipMultiplier switch
        {
          1 => 5,
          5 => 20,
          20 => 1,
          _ => 1
        };

        // Update display to highlight current multiplier
        UpdateChipMultiplierDisplay();
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void UpdateChipMultiplierDisplay()
    {
      try
      {
        // Reset all to normal size and opacity
        Chip1xText.FontSize = 16;
        Chip1xText.Opacity = 0.6;
        Chip5xText.FontSize = 16;
        Chip5xText.Opacity = 0.6;
        Chip20xText.FontSize = 16;
        Chip20xText.Opacity = 0.6;

        // Highlight current multiplier
        switch (_chipMultiplier)
        {
          case 1:
            Chip1xText.FontSize = 24;
            Chip1xText.Opacity = 1.0;
            break;
          case 5:
            Chip5xText.FontSize = 24;
            Chip5xText.Opacity = 1.0;
            break;
          case 20:
            Chip20xText.FontSize = 24;
            Chip20xText.Opacity = 1.0;
            break;
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void ShowChangeBreakdown(double changeAmount)
    {
      try
      {
        if (ChangeBreakdownPanel == null || ChangeBreakdownList == null) return;
        
        ChangeBreakdownPanel.Visibility = Visibility.Visible;
        ChangeBreakdownList.Children.Clear();
        
        // Calculate change breakdown using minimum change algorithm
        var changeBreakdown = CalculateMinimumChange(changeAmount);
        
        foreach (var kvp in changeBreakdown.OrderByDescending(x => x.Key))
        {
          if (kvp.Value > 0)
          {
            var changeItem = new StackPanel
            {
              Orientation = Orientation.Horizontal,
              Margin = new Thickness(0, 2, 0, 2)
            };
            
            var denomText = new TextBlock
            {
              Text = FormatDenomination(kvp.Key),
              Width = 60,
              FontWeight = FontWeights.Bold
            };
            
            var countText = new TextBlock
            {
              Text = $"x{kvp.Value}",
              Width = 40,
              HorizontalAlignment = HorizontalAlignment.Center
            };
            
            var valueText = new TextBlock
            {
              Text = $"{(kvp.Key * kvp.Value / 100.0):C}",
              Width = 60,
              HorizontalAlignment = HorizontalAlignment.Right,
              FontWeight = FontWeights.Bold
            };
            
            changeItem.Children.Add(denomText);
            changeItem.Children.Add(countText);
            changeItem.Children.Add(valueText);
            ChangeBreakdownList.Children.Add(changeItem);
          }
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void HideChangeBreakdown()
    {
      try
      {
        if (ChangeBreakdownPanel != null)
        {
          ChangeBreakdownPanel.Visibility = Visibility.Collapsed;
        }
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private Dictionary<int, int> CalculateMinimumChange(double changeAmount)
    {
      var breakdown = new Dictionary<int, int>();
      int remainingCents = (int)Math.Round(changeAmount * 100);

      // Australian currency denominations in descending order
      int[] denominations = { 10000, 5000, 2000, 1000, 500, 200, 100, 50, 20, 10, 5 };

      foreach (int denom in denominations)
      {
        if (remainingCents >= denom)
        {
          int count = remainingCents / denom;
          breakdown[denom] = count;
          remainingCents -= count * denom;
        }
      }

      return breakdown;
    }

    private void PaymentPlayerIdTextBox_TextChanged(object sender, TextChangedEventArgs e)
    {
      // Optional field - no action needed
    }

    private void BackToFood_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        PaymentView.Visibility = Visibility.Collapsed;
        MainFoodView.Visibility = Visibility.Visible;
        _selectedProductForPayment = null;
        _cashDenominations.Clear();
        _chipDenominations.Clear();
        _isCashMode = false; // Reset to chip mode (default)
        HideChangeBreakdown();
        UpdateTotalMoneyDisplay();
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void ConfirmPayment_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        if (_selectedProductForPayment == null)
        {
          MessageBox.Show("No product selected for payment.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
          return;
        }

        var currentDenominations = _isCashMode ? _cashDenominations : _chipDenominations;
        var paymentTotal = currentDenominations.Sum(kvp => (kvp.Key * kvp.Value) / 100.0);
        var productTotal = _selectedProductForPayment.Price;
        
        if (paymentTotal < productTotal)
        {
          MessageBox.Show("Payment amount is less than product price.", "Insufficient Payment", MessageBoxButton.OK, MessageBoxImage.Warning);
          return;
        }

        var playerId = string.IsNullOrWhiteSpace(PaymentPlayerIdTextBox?.Text) ? null : PaymentPlayerIdTextBox.Text.Trim();
        var paymentMethod = _isCashMode ? "CASH" : "CHIP";
        
        // Process the sale with database logging and cashbox deduction
        ProcessSale(playerId, currentDenominations, paymentMethod, productTotal);

        // Return to main food view
        BackToFood_Click(sender, e);
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error processing payment: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void ProcessSale(string? playerId, Dictionary<int, int> paymentDenominations, string paymentMethod, double productPrice)
    {
      try
      {
        if (Database.Conn == null) return;

        var changeAmount = paymentDenominations.Sum(kvp => (kvp.Key * kvp.Value) / 100.0) - productPrice;
        var changeBreakdown = new Dictionary<int, int>();

        // Calculate change breakdown for cash payments
        if (_isCashMode && changeAmount > 0)
        {
          changeBreakdown = CalculateMinimumChange(changeAmount);
        }

        // Use transaction to ensure data consistency
        Database.InTransaction(tx =>
        {
          // Create sale record
          var saleId = Database.ScalarLong(
            "INSERT INTO sales(player_id, staff, notes) VALUES ($player, $staff, $notes); SELECT last_insert_rowid();",
            tx,
            ("$player", (object?)playerId ?? DBNull.Value),
            ("$staff", Environment.UserName),
            ("$notes", "Food sale")
          );

          // Add sale item
          Database.Exec(
            "INSERT INTO sale_items(sale_id, product_id, qty, unit_price) VALUES ($sale, $product, $qty, $price)",
            tx,
            ("$sale", saleId),
            ("$product", _selectedProductForPayment!.Id),
            ("$qty", 1),
            ("$price", productPrice)
          );

          // Add payment record
          foreach (var kvp in paymentDenominations)
          {
            Database.Exec(
              "INSERT INTO sale_payments(sale_id, method, denom_cents, qty) VALUES ($sale, $method, $denom, $qty)",
              tx,
              ("$sale", saleId),
              ("$method", paymentMethod),
              ("$denom", kvp.Key),
              ("$qty", kvp.Value)
            );
          }

          // Update cashbox for cash payments
          if (_isCashMode)
          {
            // Add cash received to cashbox
            foreach (var kvp in paymentDenominations)
            {
              Database.Exec(
                "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id, notes) VALUES ($denom, $delta, $reason, $player, $tx, $notes)",
                tx,
                ("$denom", kvp.Key),
                ("$delta", kvp.Value), // Positive for cash received
                ("$reason", "SALE"),
                ("$player", (object?)playerId ?? DBNull.Value),
                ("$tx", (object?)saleId ?? DBNull.Value),
                ("$notes", $"Food sale - {_selectedProductForPayment.Name}")
              );
            }

            // Deduct change from cashbox
            foreach (var kvp in changeBreakdown)
            {
              Database.Exec(
                "INSERT INTO cashbox_movements(denom_cents, delta_qty, reason, player_id, tx_id, notes) VALUES ($denom, $delta, $reason, $player, $tx, $notes)",
                tx,
                ("$denom", kvp.Key),
                ("$delta", -kvp.Value), // Negative for cash given out
                ("$reason", "SALE"),
                ("$player", (object?)playerId ?? DBNull.Value),
                ("$tx", (object?)saleId ?? DBNull.Value),
                ("$notes", $"Food sale change - {_selectedProductForPayment.Name}")
              );
            }
          }

          // Log activity
          Database.Exec(
            "INSERT INTO activity_log(activity_key, activity_type, activity_kind, method, staff, player_id, tx_id, amount_cents, notes) VALUES ($key, $type, $kind, $method, $staff, $player, $tx, $amount, $notes)",
            tx,
            ("$key", $"sale_{saleId}"),
            ("$type", "SALE"),
            ("$kind", "FOOD"),
            ("$method", paymentMethod),
            ("$staff", Environment.UserName),
            ("$player", (object?)playerId ?? DBNull.Value),
            ("$tx", saleId),
            ("$amount", (int)(productPrice * 100)),
            ("$notes", $"Food sale: {_selectedProductForPayment.Name}")
          );
        });
        
        // Request activity log refresh
        ActivityRefreshRequested?.Invoke(this, EventArgs.Empty);
        
        // Update total money display
        UpdateTotalMoneyDisplay();
      }
      catch (Exception ex)
      {
        // Log error but don't throw to prevent UI issues
        System.Diagnostics.Debug.WriteLine($"Error processing sale: {ex.Message}");
      }
    }

    private void BackToMain_Click(object sender, RoutedEventArgs e)
    {
      BackToMain?.Invoke(this, EventArgs.Empty);
    }

    private void GoToSettings_Click(object sender, RoutedEventArgs e)
    {
      GoToSettings?.Invoke(this, EventArgs.Empty);
    }


    public void UpdateTotalMoneyDisplay()
    {
      try
      {
        // Calculate total money made from all food sales (including cashout food sales)
        var totalCents = Database.ScalarLong(@"
SELECT COALESCE(SUM(
  CASE 
    WHEN activity_kind = 'FOOD' THEN amount_cents
    WHEN activity_kind = 'TX' AND activity_type = 'CASHOUT' AND notes LIKE '%Food:%' 
    THEN CAST(SUBSTR(notes, INSTR(notes, 'Food: $') + 7, INSTR(notes || ' |', ' |') - INSTR(notes, 'Food: $') - 7) AS REAL) * 100
    ELSE 0
  END
), 0)
FROM activity_log 
WHERE activity_kind = 'FOOD' 
   OR (activity_kind = 'TX' AND activity_type = 'CASHOUT' AND notes LIKE '%Food:%')");

        var totalAmount = totalCents / 100.0;
        var AU = CultureInfo.GetCultureInfo("en-AU");
        TotalMoneyAmount.Text = totalAmount.ToString("C", AU);
      }
      catch (Exception ex)
      {
        TotalMoneyAmount.Text = "$0.00";
        System.Diagnostics.Debug.WriteLine($"Error updating total money: {ex.Message}");
      }
    }
  }

}