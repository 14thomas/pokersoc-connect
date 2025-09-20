using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace pokersoc_connect.Views
{
  public partial class FoodSellView : UserControl
  {
    public event EventHandler? Back;
    public event EventHandler<SaleConfirmedEventArgs>? Confirmed;

    private sealed class ProductRow { public long product_id; public string name=""; public double base_price; }
    private sealed class VariantRow { public long variant_id; public string name=""; public double price; }

    // Your denominations (chips and cash share the same cents scale)
    private static readonly int[] ChipDenoms = { 5,25,100,200,500,2500,10000 };
    private static readonly int[] CashDenoms = { 5,10,20,50,100,200,500,2500,10000 };

    private readonly CultureInfo AU = CultureInfo.GetCultureInfo("en-AU");

    public FoodSellView()
    {
      InitializeComponent();
      LoadProducts();
      Recalc();
    }

    private void LoadProducts()
    {
      var p = Database.Query("SELECT product_id,name,base_price FROM products ORDER BY name");
      ProductBox.ItemsSource = p.Rows.Cast<DataRow>()
        .Select(r => new ProductRow {
          product_id = Convert.ToInt64(r["product_id"]),
          name = r["name"]?.ToString() ?? "",
          base_price = Convert.ToDouble(r["base_price"] ?? 0)
        }).ToList();

      VariantBox.ItemsSource = null;
      UpdateDenomBox();
    }

    private void UpdateDenomBox()
    {
      var denoms = PayCash.IsChecked == true ? CashDenoms : ChipDenoms;
      DenomBox.ItemsSource = denoms.Select(d => new { cents = d, label = DenomLabel(d) }).ToList();
      DenomBox.SelectedIndex = 0;
    }

    private void ProductBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      VariantBox.ItemsSource = null;
      PriceText.Text = "";

      if (ProductBox.SelectedItem is ProductRow pr)
      {
        var v = Database.Query("SELECT variant_id,name,price FROM product_variants WHERE product_id=$p ORDER BY name", ("$p", pr.product_id));
        var rows = v.Rows.Cast<DataRow>()
           .Select(r => new VariantRow {
             variant_id = Convert.ToInt64(r["variant_id"]),
             name = r["name"]?.ToString() ?? "",
             price = Convert.ToDouble(r["price"] ?? 0)
           }).ToList();

        VariantBox.ItemsSource = rows;
        PriceText.Text = pr.base_price.ToString("C", AU);
      }
      Recalc();
    }

    private double SelectedUnitPrice()
    {
      if (VariantBox.SelectedItem is VariantRow vr) return vr.price;
      if (ProductBox.SelectedItem is ProductRow pr) return pr.base_price;
      return 0;
    }

    private int ParseInt(TextBox box, int def = 0) => int.TryParse(box.Text, out var v) ? Math.Max(0, v) : def;

    private void Recalc()
    {
      var qty = ParseInt(QtyBox, 1);
      var price = SelectedUnitPrice();
      var itemTotal = qty * price;

      ItemTotalText.Text = $"Items: {itemTotal.ToString("C", AU)}";

      var payQty = ParseInt(PayQtyBox, 1);
      var denom = (DenomBox.SelectedItem as dynamic)?.cents ?? 0;
      var payTotal = (denom * payQty) / 100.0;
      PayTotalText.Text = $"Payment: {payTotal.ToString("C", AU)}";

      var change = payTotal - itemTotal; // if negative, still owing
      var sign = change >= 0 ? "Change" : "Owing";
      ChangeText.Text = $"{sign}: {Math.Abs(change).ToString("C", AU)}";
      PriceText.Text = SelectedUnitPrice().ToString("C", AU);
    }

    private void Back_Click(object sender, RoutedEventArgs e) => Back?.Invoke(this, EventArgs.Empty);

    private void Confirm_Click(object sender, RoutedEventArgs e)
    {
      if (ProductBox.SelectedItem is not ProductRow pr)
      {
        MessageBox.Show("Pick a product."); return;
      }
      int qty = ParseInt(QtyBox, 1);
      if (qty <= 0) { MessageBox.Show("Qty must be > 0."); return; }

      double unit = SelectedUnitPrice();
      double total = qty * unit;

      int payQty = ParseInt(PayQtyBox, 1);
      if (payQty <= 0) { MessageBox.Show("Payment qty must be > 0."); return; }
      int denom = (DenomBox.SelectedItem as dynamic)?.cents ?? 0;
      double payTotal = (denom * payQty) / 100.0;

      string? playerId = (NoCardBox.IsChecked == true || string.IsNullOrWhiteSpace(PlayerBox.Text)) ? null : PlayerBox.Text.Trim();

      if (PayCash.IsChecked == true)
      {
        // For simplicity: require payment >= total (no partial for now)
        if (payTotal < total)
        {
          MessageBox.Show("Payment is less than total.");
          return;
        }
      }
      else
      {
        // chip payment: require exact or greater; chips are discrete
        if (payTotal < total)
        {
          MessageBox.Show("Chip value is less than total.");
          return;
        }
      }

      long? variantId = (VariantBox.SelectedItem is VariantRow vr) ? vr.variant_id : null;

      Confirmed?.Invoke(this, new SaleConfirmedEventArgs(
        playerId: playerId,
        productId: pr.product_id,
        variantId: variantId,
        qty: qty,
        unitPrice: unit,
        payMethod: PayCash.IsChecked == true ? "CASH" : "CHIP",
        payDenomCents: denom,
        payQty: payQty
      ));
    }

    // events to recompute
    private void AnyChanged(object? s, RoutedEventArgs e) => Recalc();
    private void Denom_Changed(object s, SelectionChangedEventArgs e) => Recalc();
    private void Qty_TextChanged(object s, TextChangedEventArgs e) => Recalc();

    private static string DenomLabel(int cents) => cents switch
    {
      5=>"5c",10=>"10c",20=>"20c",50=>"50c",
      25=>"25c",
      100=>"$1",200=>"$2",500=>"$5",2500=>"$25",10000=>"$100",
      _=>$"{cents}c"
    };

    private void PayCash_Checked(object sender, RoutedEventArgs e) { UpdateDenomBox(); Recalc(); }
    private void PayChip_Checked(object sender, RoutedEventArgs e) { UpdateDenomBox(); Recalc(); }
  }

  public sealed class SaleConfirmedEventArgs : EventArgs
  {
    public string? PlayerId { get; }
    public long ProductId { get; }
    public long? VariantId { get; }
    public int Qty { get; }
    public double UnitPrice { get; }
    public string PayMethod { get; } // CASH or CHIP
    public int PayDenomCents { get; }
    public int PayQty { get; }

    public SaleConfirmedEventArgs(string? playerId, long productId, long? variantId,
      int qty, double unitPrice, string payMethod, int payDenomCents, int payQty)
    {
      PlayerId = playerId; ProductId = productId; VariantId = variantId;
      Qty = qty; UnitPrice = unitPrice; PayMethod = payMethod;
      PayDenomCents = payDenomCents; PayQty = payQty;
    }
  }
}
