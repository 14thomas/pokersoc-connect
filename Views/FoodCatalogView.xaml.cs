using Microsoft.Win32;
using System;
using System.Data;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace pokersoc_connect.Views
{
  public partial class FoodCatalogView : UserControl
  {
    public event EventHandler? RequestSell;
    public FoodCatalogView()
    {
      InitializeComponent();
      LoadProducts();
    }

    private void LoadProducts()
    {
      var p = Database.Query("SELECT product_id, name, base_price, image_path FROM products ORDER BY name");
      ProductsGrid.ItemsSource = p.AsEnumerable().Select(r => new
      {
        product_id = Convert.ToInt64(r["product_id"]),
        name = r["name"]?.ToString() ?? "",
        base_price = Convert.ToDouble(r["base_price"] ?? 0).ToString("C", CultureInfo.GetCultureInfo("en-AU")),
        image_path = r["image_path"] == DBNull.Value ? "" : r["image_path"]?.ToString() ?? ""
      }).ToList();

      VariantsGrid.ItemsSource = null;
    }

    private void LoadVariants(long productId)
    {
      var v = Database.Query("SELECT variant_id, name, price, image_path FROM product_variants WHERE product_id=$p ORDER BY name", ("$p", productId));
      VariantsGrid.ItemsSource = v.AsEnumerable().Select(r => new
      {
        variant_id = Convert.ToInt64(r["variant_id"]),
        name = r["name"]?.ToString() ?? "",
        price = Convert.ToDouble(r["price"] ?? 0).ToString("C", CultureInfo.GetCultureInfo("en-AU")),
        image_path = r["image_path"] == DBNull.Value ? "" : r["image_path"]?.ToString() ?? ""
      }).ToList();
    }

    private void ProductsGrid_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      if (ProductsGrid.SelectedItem is null) { VariantsGrid.ItemsSource = null; return; }
      var row = ProductsGrid.SelectedItem;
      long productId = (long)row.GetType().GetProperty("product_id")!.GetValue(row, null)!;
      LoadVariants(productId);
    }

    private void New_Click(object sender, RoutedEventArgs e)
    {
      var dlg = new ProductDialog(); // simple dialog you can create: fields: name, base price, image path
      if (dlg.ShowDialog() == true)
      {
        Database.Exec("INSERT INTO products(name,base_price,image_path) VALUES ($n,$p,$img)",
          ("$n", dlg.ProductName), ("$p", dlg.BasePrice), ("$img", (object?)dlg.ImagePath ?? DBNull.Value));
        LoadProducts();
      }
    }

    private void NewVariant_Click(object sender, RoutedEventArgs e)
    {
      if (ProductsGrid.SelectedItem is null) { MessageBox.Show("Pick a product first."); return; }
      var row = ProductsGrid.SelectedItem;
      long productId = (long)row.GetType().GetProperty("product_id")!.GetValue(row, null)!;

      var dlg = new VariantDialog(); // fields: name, price, image path
      if (dlg.ShowDialog() == true)
      {
        Database.Exec("INSERT INTO product_variants(product_id,name,price,image_path) VALUES ($p,$n,$pr,$img)",
          ("$p", productId), ("$n", dlg.VariantName), ("$pr", dlg.Price), ("$img", (object?)dlg.ImagePath ?? DBNull.Value));
        LoadVariants(productId);
      }
    }

    private void Edit_Click(object sender, RoutedEventArgs e)
    {
      if (VariantsGrid.SelectedItem != null)
      {
        var vrow = VariantsGrid.SelectedItem;
        long variantId = (long)vrow.GetType().GetProperty("variant_id")!.GetValue(vrow, null)!;

        var vdb = Database.Query("SELECT name, price, image_path FROM product_variants WHERE variant_id=$id", ("$id", variantId));
        if (vdb.Rows.Count == 1)
        {
          var r = vdb.Rows[0];
          var dlg = new VariantDialog
          {
            VariantName = r["name"]?.ToString() ?? "",
            Price = Convert.ToDouble(r["price"] ?? 0),
            ImagePath = r["image_path"] == DBNull.Value ? null : r["image_path"]?.ToString()
          };
          if (dlg.ShowDialog() == true)
          {
            Database.Exec("UPDATE product_variants SET name=$n, price=$p, image_path=$img WHERE variant_id=$id",
              ("$n", dlg.VariantName), ("$p", dlg.Price), ("$img", (object?)dlg.ImagePath ?? DBNull.Value), ("$id", variantId));
            // refresh
            if (ProductsGrid.SelectedItem is not null)
            {
              long pid = (long)ProductsGrid.SelectedItem.GetType().GetProperty("product_id")!.GetValue(ProductsGrid.SelectedItem, null)!;
              LoadVariants(pid);
            }
          }
        }
        return;
      }

      if (ProductsGrid.SelectedItem != null)
      {
        var prow = ProductsGrid.SelectedItem;
        long productId = (long)prow.GetType().GetProperty("product_id")!.GetValue(prow, null)!;
        var pdb = Database.Query("SELECT name, base_price, image_path FROM products WHERE product_id=$id", ("$id", productId));
        if (pdb.Rows.Count == 1)
        {
          var r = pdb.Rows[0];
          var dlg = new ProductDialog
          {
            ProductName = r["name"]?.ToString() ?? "",
            BasePrice = Convert.ToDouble(r["base_price"] ?? 0),
            ImagePath = r["image_path"] == DBNull.Value ? null : r["image_path"]?.ToString()
          };
          if (dlg.ShowDialog() == true)
          {
            Database.Exec("UPDATE products SET name=$n, base_price=$p, image_path=$img WHERE product_id=$id",
              ("$n", dlg.ProductName), ("$p", dlg.BasePrice), ("$img", (object?)dlg.ImagePath ?? DBNull.Value), ("$id", productId));
            LoadProducts();
          }
        }
      }
    }

    private void Delete_Click(object sender, RoutedEventArgs e)
    {
      if (VariantsGrid.SelectedItem != null)
      {
        var vrow = VariantsGrid.SelectedItem;
        long variantId = (long)vrow.GetType().GetProperty("variant_id")!.GetValue(vrow, null)!;
        Database.Exec("DELETE FROM product_variants WHERE variant_id=$id", ("$id", variantId));
        if (ProductsGrid.SelectedItem is not null)
        {
          long pid = (long)ProductsGrid.SelectedItem.GetType().GetProperty("product_id")!.GetValue(ProductsGrid.SelectedItem, null)!;
          LoadVariants(pid);
        }
        return;
      }
      if (ProductsGrid.SelectedItem != null)
      {
        var prow = ProductsGrid.SelectedItem;
        long productId = (long)prow.GetType().GetProperty("product_id")!.GetValue(prow, null)!;
        if (MessageBox.Show("Delete product and its variants?", "Confirm", MessageBoxButton.YesNo) == MessageBoxResult.Yes)
        {
          Database.Exec("DELETE FROM products WHERE product_id=$id", ("$id", productId));
          LoadProducts();
        }
      }
    }


    private void Sell_Click(object sender, RoutedEventArgs e)
    {
      RequestSell?.Invoke(this, EventArgs.Empty);
    }
  }
}
