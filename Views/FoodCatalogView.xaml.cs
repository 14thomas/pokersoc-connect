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
    public event EventHandler<SaleConfirmedEventArgs>? SaleConfirmed;

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
    private Dictionary<int, int> _paymentDenominations = new(); // denomination -> quantity
    private int _selectedSlotIndex = -1; // for product selection

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
        LoadRecentSales();
        LoadPaymentButtons();
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
        _cashButtons[0] = Payment5c; _cashCounts[0] = Payment5cCount;
        _cashButtons[1] = Payment10c; _cashCounts[1] = Payment10cCount;
        _cashButtons[2] = Payment20c; _cashCounts[2] = Payment20cCount;
        _cashButtons[3] = Payment50c; _cashCounts[3] = Payment50cCount;
        _cashButtons[4] = Payment1; _cashCounts[4] = Payment1Count;
        _cashButtons[5] = Payment2; _cashCounts[5] = Payment2Count;
        _cashButtons[6] = Payment5; _cashCounts[6] = Payment5Count;
        _cashButtons[7] = Payment10; _cashCounts[7] = Payment10Count;
        _cashButtons[8] = Payment20; _cashCounts[8] = Payment20Count;
        _cashButtons[9] = Payment50; _cashCounts[9] = Payment50Count;
        _cashButtons[10] = Payment100; _cashCounts[10] = Payment100Count;

        // Chip buttons (reuse same buttons, different denominations)
        _chipButtons[0] = Payment5c; _chipCounts[0] = Payment5cCount; // 5c
        _chipButtons[1] = Payment50c; _chipCounts[1] = Payment50cCount; // 25c (reuse 50c button)
        _chipButtons[2] = Payment1; _chipCounts[2] = Payment1Count; // $1
        _chipButtons[3] = Payment2; _chipCounts[3] = Payment2Count; // $2
        _chipButtons[4] = Payment5; _chipCounts[4] = Payment5Count; // $5
        _chipButtons[5] = Payment50; _chipCounts[5] = Payment50Count; // $25 (reuse $50 button)
        _chipButtons[6] = Payment100; _chipCounts[6] = Payment100Count; // $100
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
        
        // Get all item buttons and their text blocks
        var buttons = new[] { ItemButton1, ItemButton2, ItemButton3, ItemButton4, ItemButton5, ItemButton6 };
        var nameTexts = new[] { Item1Name, Item2Name, Item3Name, Item4Name, Item5Name, Item6Name };
        var priceTexts = new[] { Item1Price, Item2Price, Item3Price, Item4Price, Item5Price, Item6Price };
        var iconTexts = new[] { Item1Icon, Item2Icon, Item3Icon, Item4Icon, Item5Icon, Item6Icon };

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

    private void LoadRecentSales()
    {
      try
      {
        if (RecentSalesPanel == null) return;
        
        if (Database.Conn == null) 
        {
          ShowNoSalesMessage();
          return;
        }

        RecentSalesPanel.Children.Clear();

        // Check if sales tables exist first
        var tableCheck = Database.Query("SELECT name FROM sqlite_master WHERE type='table' AND name IN ('sales', 'sale_items', 'sale_payments')");
        if (tableCheck == null || tableCheck.Rows.Count < 3)
        {
          ShowNoSalesMessage();
          return;
        }

        var query = @"
          SELECT 
            s.time,
            p.name as product_name,
            COALESCE(s.player_id, 'Anonymous') as player_id,
            si.qty * si.unit_price as total_amount,
            sp.method
          FROM sales s
          JOIN sale_items si ON s.sale_id = si.sale_id
          JOIN products p ON si.product_id = p.product_id
          LEFT JOIN sale_payments sp ON s.sale_id = sp.sale_id
          ORDER BY s.time DESC
          LIMIT 10";

        var result = Database.Query(query);
        if (result == null || result.Rows.Count == 0)
        {
          ShowNoSalesMessage();
          return;
        }

        foreach (DataRow row in result.Rows)
        {
          var salePanel = new StackPanel
          {
            Margin = new Thickness(4, 2, 4, 2),
            Background = Brushes.LightGray
          };

          var timeText = new TextBlock
          {
            Text = Convert.ToDateTime(row["time"]).ToString("HH:mm"),
            FontWeight = FontWeights.Bold,
            FontSize = 12
          };

          var productText = new TextBlock
          {
            Text = $"{row["product_name"]} x{row["qty"]}",
            FontSize = 11
          };

          var detailsText = new TextBlock
          {
            Text = $"{row["player_id"]} - {Convert.ToDouble(row["total_amount"]):C} ({row["method"]})",
            FontSize = 10,
            Foreground = Brushes.DarkGray
          };

          salePanel.Children.Add(timeText);
          salePanel.Children.Add(productText);
          salePanel.Children.Add(detailsText);
          RecentSalesPanel.Children.Add(salePanel);
        }
      }
      catch (Exception ex)
      {
        RecentSalesPanel.Children.Clear();
        ShowNoSalesMessage();
      }
    }

    private void ShowNoSalesMessage()
    {
      if (RecentSalesPanel == null) return;
      
      var noSalesText = new TextBlock
      {
        Text = "No recent sales",
        FontStyle = FontStyles.Italic,
        Foreground = Brushes.Gray,
        Margin = new Thickness(0, 4, 0, 4)
      };
      RecentSalesPanel.Children.Add(noSalesText);
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
        _paymentDenominations.Clear();
        
        // Update payment screen
        PaymentProductName.Text = product.Name;
        PaymentProductPrice.Text = product.Price.ToString("C", AU);
        PaymentPlayerIdTextBox.Text = "";
        
        // Show payment screen
        MainFoodView.Visibility = Visibility.Collapsed;
        PaymentView.Visibility = Visibility.Visible;
        
        // Initialize payment buttons
        PaymentClear_Click(this, new RoutedEventArgs()); // Clear all counts
        UpdatePaymentButtonLabels();
        UpdatePaymentSummary();
      }
      catch (Exception ex)
      {
        // Silently handle errors for fullscreen mode
      }
    }

    private void UpdatePaymentButtonLabels()
    {
      try
      {
        var isCash = PaymentCashRadioButton?.IsChecked == true;
        
        if (isCash)
        {
          // Cash denominations
          Payment50cText.Text = "50c";
          Payment50cLabel.Text = "50c";
          Payment50c.Tag = "50";
          
          Payment50Text.Text = "$50";
          Payment50Label.Text = "$50";
          Payment50.Tag = "5000";
        }
        else
        {
          // Chip denominations
          Payment50cText.Text = "25c";
          Payment50cLabel.Text = "25c";
          Payment50c.Tag = "25";
          
          Payment50Text.Text = "$25";
          Payment50Label.Text = "$25";
          Payment50.Tag = "2500";
        }
        
        // Hide/show buttons based on payment method
        Payment10c.Visibility = isCash ? Visibility.Visible : Visibility.Collapsed;
        Payment20c.Visibility = isCash ? Visibility.Visible : Visibility.Collapsed;
        Payment10.Visibility = isCash ? Visibility.Visible : Visibility.Collapsed;
        Payment20.Visibility = isCash ? Visibility.Visible : Visibility.Collapsed;
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
    }

    private void PaymentDenom_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        if (sender is Button button && int.TryParse(button.Tag?.ToString(), out int denomination))
        {
          // Find the count text block for this button
          TextBlock? countText = null;
          
          if (button == Payment5c) countText = Payment5cCount;
          else if (button == Payment10c) countText = Payment10cCount;
          else if (button == Payment20c) countText = Payment20cCount;
          else if (button == Payment50c) countText = Payment50cCount;
          else if (button == Payment1) countText = Payment1Count;
          else if (button == Payment2) countText = Payment2Count;
          else if (button == Payment5) countText = Payment5Count;
          else if (button == Payment10) countText = Payment10Count;
          else if (button == Payment20) countText = Payment20Count;
          else if (button == Payment50) countText = Payment50Count;
          else if (button == Payment100) countText = Payment100Count;
          
          if (countText != null)
          {
            var currentCount = int.Parse(countText.Text);
            countText.Text = (currentCount + 1).ToString();
            
            if (_paymentDenominations.ContainsKey(denomination))
            {
              _paymentDenominations[denomination]++;
            }
            else
            {
              _paymentDenominations[denomination] = 1;
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

    private void PaymentClear_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        _paymentDenominations.Clear();
        
        // Reset all count displays
        Payment5cCount.Text = "0";
        Payment10cCount.Text = "0";
        Payment20cCount.Text = "0";
        Payment50cCount.Text = "0";
        Payment1Count.Text = "0";
        Payment2Count.Text = "0";
        Payment5Count.Text = "0";
        Payment10Count.Text = "0";
        Payment20Count.Text = "0";
        Payment50Count.Text = "0";
        Payment100Count.Text = "0";
        
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
        if (_selectedProductForPayment == null || PaymentSummaryText == null || PaymentChangeText == null || ConfirmPaymentButton == null)
          return;
        
        var productTotal = _selectedProductForPayment.Price;
        var paymentTotal = _paymentDenominations.Sum(kvp => (kvp.Key * kvp.Value) / 100.0);
        var change = paymentTotal - productTotal;
        
        PaymentSummaryText.Text = $"Product: {productTotal.ToString("C", AU)} | Payment: {paymentTotal.ToString("C", AU)}";
        
        if (change >= 0)
        {
          PaymentChangeText.Text = $"Change: {change.ToString("C", AU)}";
          PaymentChangeText.Foreground = Brushes.Green;
          ConfirmPaymentButton.IsEnabled = true;
          ConfirmPaymentButton.Opacity = 1.0;
        }
        else
        {
          PaymentChangeText.Text = $"Owing: {Math.Abs(change).ToString("C", AU)}";
          PaymentChangeText.Foreground = Brushes.Red;
          ConfirmPaymentButton.IsEnabled = false;
          ConfirmPaymentButton.Opacity = 0.6;
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

    private void PaymentMethod_Changed(object sender, RoutedEventArgs e)
    {
      try
      {
        _paymentDenominations.Clear();
        PaymentClear_Click(sender, e); // Clear all counts
        UpdatePaymentButtonLabels();
        UpdatePaymentSummary();
      }
      catch (Exception ex)
      {
        // Silently handle errors
      }
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
        _paymentDenominations.Clear();
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

        var paymentTotal = _paymentDenominations.Sum(kvp => (kvp.Key * kvp.Value) / 100.0);
        var productTotal = _selectedProductForPayment.Price;
        
        if (paymentTotal < productTotal)
        {
          MessageBox.Show("Payment amount is less than product price.", "Insufficient Payment", MessageBoxButton.OK, MessageBoxImage.Warning);
          return;
        }

        var playerId = string.IsNullOrWhiteSpace(PaymentPlayerIdTextBox?.Text) ? null : PaymentPlayerIdTextBox.Text.Trim();
        var paymentMethod = PaymentCashRadioButton?.IsChecked == true ? "CASH" : "CHIP";
        
        // Create sale confirmed event
        var change = paymentTotal - productTotal;
        
        SaleConfirmed?.Invoke(this, new SaleConfirmedEventArgs(
          playerId: playerId,
          productId: _selectedProductForPayment.Id,
          qty: 1,
          unitPrice: _selectedProductForPayment.Price,
          payMethod: paymentMethod,
          payDenomCents: _paymentDenominations.Keys.First(),
          payQty: _paymentDenominations.Values.Sum()
        ));

        // Return to main food view
        BackToFood_Click(sender, e);
        LoadRecentSales();
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error processing payment: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
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
  }

  public sealed class SaleConfirmedEventArgs : EventArgs
  {
    public string? PlayerId { get; }
    public long ProductId { get; }
    public int Qty { get; }
    public double UnitPrice { get; }
    public string PayMethod { get; }
    public int PayDenomCents { get; }
    public int PayQty { get; }

    public SaleConfirmedEventArgs(string? playerId, long productId, int qty, double unitPrice, string payMethod, int payDenomCents, int payQty)
    {
      PlayerId = playerId;
      ProductId = productId;
      Qty = qty;
      UnitPrice = unitPrice;
      PayMethod = payMethod;
      PayDenomCents = payDenomCents;
      PayQty = payQty;
    }
  }
}