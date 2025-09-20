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
  public partial class FoodSellView : UserControl
  {
    public event EventHandler? BackToCatalog;
    public event EventHandler? BackToMain;
    public event EventHandler<SaleConfirmedEventArgs>? SaleConfirmed;

    // Data classes
    private sealed class Product
    {
      public long Id { get; set; }
      public string Name { get; set; } = "";
      public double BasePrice { get; set; }
    }

    private sealed class Variant
    {
      public long Id { get; set; }
      public string Name { get; set; } = "";
      public double Price { get; set; }
    }

    private sealed class Denomination
    {
      public int Cents { get; set; }
      public string Label { get; set; } = "";
    }

    // Constants
    private static readonly int[] CashDenominations = { 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    private static readonly int[] ChipDenominations = { 5, 25, 100, 200, 500, 2500, 10000 };
    private static readonly CultureInfo AU = CultureInfo.GetCultureInfo("en-AU");

    // State
    private List<Product> _products = new();
    private List<Variant> _variants = new();
    private List<Denomination> _denominations = new();
    private Product? _selectedProduct;
    private Variant? _selectedVariant;
    private Denomination? _selectedDenomination;

    public FoodSellView()
    {
      InitializeComponent();
      Loaded += (s, e) => InitializeView();
    }

    private void InitializeView()
    {
      try
      {
        LoadProducts();
        LoadRecentSales();
        UpdatePaymentMethod();
        UpdateSummary();
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error initializing view: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private void LoadProducts()
    {
      try
      {
        if (Database.Conn == null)
        {
          ShowError("No database connection");
          return;
        }

        var query = "SELECT product_id, name, base_price FROM products ORDER BY name";
        var result = Database.Query(query);
        
        if (result == null)
        {
          ShowError("Failed to load products");
          return;
        }

        _products = result.Rows.Cast<DataRow>()
          .Select(row => new Product
          {
            Id = Convert.ToInt64(row["product_id"]),
            Name = row["name"]?.ToString() ?? "",
            BasePrice = Convert.ToDouble(row["base_price"] ?? 0)
          })
          .ToList();

        ProductComboBox.ItemsSource = _products;
        ProductComboBox.DisplayMemberPath = "Name";
        ProductComboBox.SelectedValuePath = "Id";
      }
      catch (Exception ex)
      {
        ShowError($"Error loading products: {ex.Message}");
      }
    }

    private void LoadRecentSales()
    {
      try
      {
        if (Database.Conn == null) return;

        RecentSalesPanel.Children.Clear();

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
          var noSalesText = new TextBlock
          {
            Text = "No recent sales",
            FontStyle = FontStyles.Italic,
            Foreground = Brushes.Gray,
            Margin = new Thickness(0, 4, 0, 4)
          };
          RecentSalesPanel.Children.Add(noSalesText);
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
        var errorText = new TextBlock
        {
          Text = $"Error loading sales: {ex.Message}",
          Foreground = Brushes.Red,
          FontSize = 10,
          Margin = new Thickness(0, 4, 0, 4)
        };
        RecentSalesPanel.Children.Add(errorText);
      }
    }

    private void UpdatePaymentMethod()
    {
      try
      {
        var denoms = CashRadioButton.IsChecked == true ? CashDenominations : ChipDenominations;
        _denominations = denoms.Select(d => new Denomination
        {
          Cents = d,
          Label = FormatDenomination(d)
        }).ToList();

        DenominationComboBox.ItemsSource = _denominations;
        DenominationComboBox.DisplayMemberPath = "Label";
        DenominationComboBox.SelectedValuePath = "Cents";
        
        if (_denominations.Count > 0)
        {
          DenominationComboBox.SelectedIndex = 0;
        }
      }
      catch (Exception ex)
      {
        ShowError($"Error updating payment method: {ex.Message}");
      }
    }

    private void UpdateSummary()
    {
      try
      {
        var quantity = GetQuantity();
        var unitPrice = GetUnitPrice();
        var itemTotal = quantity * unitPrice;

        var paymentQuantity = GetPaymentQuantity();
        var denominationCents = GetDenominationCents();
        var paymentTotal = (denominationCents * paymentQuantity) / 100.0;

        var change = paymentTotal - itemTotal;

        ItemTotalText.Text = $"Items: {itemTotal.ToString("C", AU)}";
        PaymentTotalText.Text = $"Payment: {paymentTotal.ToString("C", AU)}";
        
        if (change >= 0)
        {
          ChangeText.Text = $"Change: {change.ToString("C", AU)}";
          ChangeText.Foreground = Brushes.Green;
        }
        else
        {
          ChangeText.Text = $"Owing: {Math.Abs(change).ToString("C", AU)}";
          ChangeText.Foreground = Brushes.Red;
        }

        // Update confirm button
        var canConfirm = quantity > 0 && unitPrice > 0 && denominationCents > 0 && paymentQuantity > 0 && paymentTotal >= itemTotal;
        ConfirmSaleButton.IsEnabled = canConfirm;
        ConfirmSaleButton.Opacity = canConfirm ? 1.0 : 0.6;
      }
      catch (Exception ex)
      {
        ShowError($"Error updating summary: {ex.Message}");
      }
    }

    private int GetQuantity()
    {
      return int.TryParse(QuantityTextBox.Text, out var qty) ? Math.Max(1, qty) : 1;
    }

    private double GetUnitPrice()
    {
      if (_selectedVariant != null) return _selectedVariant.Price;
      if (_selectedProduct != null) return _selectedProduct.BasePrice;
      return 0;
    }

    private int GetPaymentQuantity()
    {
      return int.TryParse(PaymentQuantityTextBox.Text, out var qty) ? Math.Max(1, qty) : 1;
    }

    private int GetDenominationCents()
    {
      return _selectedDenomination?.Cents ?? 0;
    }

    private string FormatDenomination(int cents)
    {
      return cents switch
      {
        5 => "5c",
        10 => "10c",
        20 => "20c",
        25 => "25c",
        50 => "50c",
        100 => "$1",
        200 => "$2",
        500 => "$5",
        1000 => "$10",
        2000 => "$20",
        2500 => "$25",
        5000 => "$50",
        10000 => "$100",
        _ => $"{cents}c"
      };
    }

    private void ShowError(string message)
    {
      MessageBox.Show(message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
    }

    // Event Handlers
    private void ProductComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      try
      {
        _selectedProduct = ProductComboBox.SelectedItem as Product;
        LoadVariants();
        UpdateUnitPrice();
        UpdateSummary();
      }
      catch (Exception ex)
      {
        ShowError($"Error selecting product: {ex.Message}");
      }
    }

    private void LoadVariants()
    {
      try
      {
        _variants.Clear();
        VariantComboBox.ItemsSource = null;

        if (_selectedProduct == null || Database.Conn == null) return;

        var query = "SELECT variant_id, name, price FROM product_variants WHERE product_id = @productId ORDER BY name";
        var result = Database.Query(query, ("@productId", _selectedProduct.Id));

        if (result != null)
        {
          _variants = result.Rows.Cast<DataRow>()
            .Select(row => new Variant
            {
              Id = Convert.ToInt64(row["variant_id"]),
              Name = row["name"]?.ToString() ?? "",
              Price = Convert.ToDouble(row["price"] ?? 0)
            })
            .ToList();

          VariantComboBox.ItemsSource = _variants;
          VariantComboBox.DisplayMemberPath = "Name";
          VariantComboBox.SelectedValuePath = "Id";
        }
      }
      catch (Exception ex)
      {
        ShowError($"Error loading variants: {ex.Message}");
      }
    }

    private void VariantComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      try
      {
        _selectedVariant = VariantComboBox.SelectedItem as Variant;
        UpdateUnitPrice();
        UpdateSummary();
      }
      catch (Exception ex)
      {
        ShowError($"Error selecting variant: {ex.Message}");
      }
    }

    private void UpdateUnitPrice()
    {
      var price = GetUnitPrice();
      UnitPriceText.Text = price.ToString("C", AU);
    }

    private void QuantityTextBox_TextChanged(object sender, TextChangedEventArgs e)
    {
      UpdateSummary();
    }

    private void PaymentMethod_Changed(object sender, RoutedEventArgs e)
    {
      UpdatePaymentMethod();
      UpdateSummary();
    }

    private void DenominationComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      try
      {
        _selectedDenomination = DenominationComboBox.SelectedItem as Denomination;
        UpdateSummary();
      }
      catch (Exception ex)
      {
        ShowError($"Error selecting denomination: {ex.Message}");
      }
    }

    private void PaymentQuantityTextBox_TextChanged(object sender, TextChangedEventArgs e)
    {
      UpdateSummary();
    }

    private void NoCardCheckBox_Changed(object sender, RoutedEventArgs e)
    {
      PlayerIdTextBox.IsEnabled = !NoCardCheckBox.IsChecked == true;
      if (NoCardCheckBox.IsChecked == true)
      {
        PlayerIdTextBox.Text = "";
      }
    }

    private void PlayerIdTextBox_TextChanged(object sender, TextChangedEventArgs e)
    {
      // Auto-clear no card checkbox when player ID is entered
      if (!string.IsNullOrWhiteSpace(PlayerIdTextBox.Text))
      {
        NoCardCheckBox.IsChecked = false;
      }
    }

    private void BackToCatalog_Click(object sender, RoutedEventArgs e)
    {
      BackToCatalog?.Invoke(this, EventArgs.Empty);
    }

    private void BackToMain_Click(object sender, RoutedEventArgs e)
    {
      BackToMain?.Invoke(this, EventArgs.Empty);
    }

    private void ConfirmSale_Click(object sender, RoutedEventArgs e)
    {
      try
      {
        // Validate inputs
        if (_selectedProduct == null)
        {
          MessageBox.Show("Please select a product.", "Validation Error", MessageBoxButton.OK, MessageBoxImage.Warning);
          ProductComboBox.Focus();
          return;
        }

        var quantity = GetQuantity();
        if (quantity <= 0)
        {
          MessageBox.Show("Quantity must be greater than 0.", "Validation Error", MessageBoxButton.OK, MessageBoxImage.Warning);
          QuantityTextBox.Focus();
          QuantityTextBox.SelectAll();
          return;
        }

        var denominationCents = GetDenominationCents();
        if (denominationCents <= 0)
        {
          MessageBox.Show("Please select a denomination.", "Validation Error", MessageBoxButton.OK, MessageBoxImage.Warning);
          DenominationComboBox.Focus();
          return;
        }

        var paymentQuantity = GetPaymentQuantity();
        if (paymentQuantity <= 0)
        {
          MessageBox.Show("Payment quantity must be greater than 0.", "Validation Error", MessageBoxButton.OK, MessageBoxImage.Warning);
          PaymentQuantityTextBox.Focus();
          PaymentQuantityTextBox.SelectAll();
          return;
        }

        var unitPrice = GetUnitPrice();
        var total = quantity * unitPrice;
        var paymentTotal = (denominationCents * paymentQuantity) / 100.0;

        if (paymentTotal < total)
        {
          var method = CashRadioButton.IsChecked == true ? "cash" : "chip";
          MessageBox.Show($"Payment amount ({paymentTotal:C}) is less than total ({total:C}). Please increase the {method} quantity or denomination.", 
                         "Insufficient Payment", MessageBoxButton.OK, MessageBoxImage.Warning);
          PaymentQuantityTextBox.Focus();
          PaymentQuantityTextBox.SelectAll();
          return;
        }

        var playerId = NoCardCheckBox.IsChecked == true || string.IsNullOrWhiteSpace(PlayerIdTextBox.Text) 
          ? null : PlayerIdTextBox.Text.Trim();

        var variantId = _selectedVariant?.Id;

        // Show confirmation dialog
        var change = paymentTotal - total;
        var changeText = change > 0 ? $"Change: {change:C}" : "Exact payment";
        var paymentMethod = CashRadioButton.IsChecked == true ? "Cash" : "Chip";
        var playerDisplay = string.IsNullOrEmpty(playerId) ? "Anonymous" : playerId;
        
        var confirmMessage = $"Confirm sale?\n\n" +
                           $"Product: {_selectedProduct.Name}\n" +
                           $"Quantity: {quantity}\n" +
                           $"Total: {total:C}\n" +
                           $"Payment: {paymentTotal:C} ({paymentMethod})\n" +
                           $"{changeText}\n" +
                           $"Player: {playerDisplay}";

        var result = MessageBox.Show(confirmMessage, "Confirm Sale", MessageBoxButton.YesNo, MessageBoxImage.Question);
        if (result == MessageBoxResult.Yes)
        {
          SaleConfirmed?.Invoke(this, new SaleConfirmedEventArgs(
            playerId: playerId,
            productId: _selectedProduct.Id,
            variantId: variantId,
            qty: quantity,
            unitPrice: unitPrice,
            payMethod: CashRadioButton.IsChecked == true ? "CASH" : "CHIP",
            payDenomCents: denominationCents,
            payQty: paymentQuantity
          ));

          // Clear form
          ClearForm();
          LoadRecentSales();
        }
      }
      catch (Exception ex)
      {
        ShowError($"Error confirming sale: {ex.Message}");
      }
    }

    private void ClearForm()
    {
      ProductComboBox.SelectedIndex = -1;
      VariantComboBox.ItemsSource = null;
      QuantityTextBox.Text = "1";
      PaymentQuantityTextBox.Text = "1";
      PlayerIdTextBox.Text = "";
      NoCardCheckBox.IsChecked = false;
      _selectedProduct = null;
      _selectedVariant = null;
      _selectedDenomination = null;
      UpdateSummary();
    }
  }

  public sealed class SaleConfirmedEventArgs : EventArgs
  {
    public string? PlayerId { get; }
    public long ProductId { get; }
    public long? VariantId { get; }
    public int Qty { get; }
    public double UnitPrice { get; }
    public string PayMethod { get; }
    public int PayDenomCents { get; }
    public int PayQty { get; }

    public SaleConfirmedEventArgs(string? playerId, long productId, long? variantId, int qty, double unitPrice, 
                                 string payMethod, int payDenomCents, int payQty)
    {
      PlayerId = playerId;
      ProductId = productId;
      VariantId = variantId;
      Qty = qty;
      UnitPrice = unitPrice;
      PayMethod = payMethod;
      PayDenomCents = payDenomCents;
      PayQty = payQty;
    }
  }
}