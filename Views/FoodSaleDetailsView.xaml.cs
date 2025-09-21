using System;
using System.Collections.Generic;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace pokersoc_connect.Views
{
  public partial class FoodSaleDetailsView : UserControl
  {
    public event EventHandler? Back;

    public FoodSaleDetailsView(long saleId)
    {
      InitializeComponent();
      DataContext = BuildViewModel(saleId);
    }

    private object BuildViewModel(long saleId)
    {
      try
      {
        // Get sale details
        var saleDt = Database.Query(@"
SELECT s.time, s.player_id, s.staff, s.notes as sale_notes,
       p.name as product_name, si.qty, si.unit_price,
       (si.qty * si.unit_price) as total_amount
FROM sales s
JOIN sale_items si ON s.sale_id = si.sale_id
JOIN products p ON si.product_id = p.product_id
WHERE s.sale_id = $sale", ("$sale", saleId));

        if (saleDt.Rows.Count == 0)
        {
          return new { SaleTime = "Sale not found", ProductInfo = "", PaymentInfo = "", PaymentRows = new List<object>(), ChangeRows = new List<object>(), HasPayments = false, HasChange = false };
        }

        var sale = saleDt.Rows[0];
        var au = CultureInfo.GetCultureInfo("en-AU");

        // Build sale time
        var saleTime = Convert.ToDateTime(sale["time"]).ToString("yyyy-MM-dd HH:mm:ss");

        // Build product info
        var productInfo = $"{sale["product_name"]} x{sale["qty"]} @ {Convert.ToDouble(sale["unit_price"]):C} = {Convert.ToDouble(sale["total_amount"]):C}";

        // Build payment info
        var paymentInfo = $"Staff: {sale["staff"]}";
        if (sale["player_id"] != DBNull.Value && !string.IsNullOrEmpty(sale["player_id"]?.ToString()))
        {
          paymentInfo += $" | Player: {sale["player_id"]}";
        }

        // Get payment details
        var paymentDt = Database.Query(@"
SELECT method, denom_cents, qty, (denom_cents * qty / 100.0) as amount
FROM sale_payments
WHERE sale_id = $sale
ORDER BY method, denom_cents DESC", ("$sale", saleId));

        var paymentRows = paymentDt.Rows.Cast<DataRow>()
          .Select(r => new {
            Method = r["method"]?.ToString() ?? "",
            Denomination = FormatDenomination(Convert.ToInt32(r["denom_cents"])),
            Qty = Convert.ToInt32(r["qty"]),
            Amount = Convert.ToDouble(r["amount"]).ToString("C", au)
          })
          .ToList();

        // Get change details
        var cashboxDt = Database.Query(@"
SELECT denom_cents, ABS(delta_qty) AS qty
FROM cashbox_movements
WHERE tx_id = $sale AND reason = 'SALE' AND delta_qty < 0
ORDER BY denom_cents DESC", ("$sale", saleId));

        var changeRows = cashboxDt.Rows.Cast<DataRow>()
          .Select(r => new {
            Denomination = FormatDenomination(Convert.ToInt32(r["denom_cents"])),
            Qty = Convert.ToInt32(r["qty"]),
            Amount = (Convert.ToInt32(r["denom_cents"]) * Convert.ToInt32(r["qty"]) / 100.0).ToString("C", au)
          })
          .ToList();

        return new {
          SaleTime = saleTime,
          ProductInfo = productInfo,
          PaymentInfo = paymentInfo,
          PaymentRows = paymentRows,
          ChangeRows = changeRows,
          HasPayments = paymentRows.Any(),
          HasChange = changeRows.Any()
        };
      }
      catch (Exception ex)
      {
        return new { 
          SaleTime = "Error loading sale details", 
          ProductInfo = ex.Message, 
          PaymentInfo = "", 
          PaymentRows = new List<object>(), 
          ChangeRows = new List<object>(), 
          HasPayments = false, 
          HasChange = false 
        };
      }
    }

    private string FormatDenomination(int denomCents)
    {
      return denomCents >= 100 ? $"${denomCents / 100}" : $"{denomCents}c";
    }

    private void Back_Click(object sender, RoutedEventArgs e)
    {
      Back?.Invoke(this, EventArgs.Empty);
    }
  }
}
