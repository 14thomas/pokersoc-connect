using Microsoft.Win32;
using System;
using System.Globalization;
using System.Windows;

namespace pokersoc_connect.Views
{
  public partial class ProductDialog : Window
  {
    public string ProductName { get; set; } = "";
    public double BasePrice { get; set; }
    public string? ImagePath { get; set; }

    private readonly CultureInfo AU = CultureInfo.GetCultureInfo("en-AU");

    public ProductDialog()
    {
      InitializeComponent();
      Loaded += (_, __) =>
      {
        NameBox.Text = ProductName;
        PriceBox.Text = BasePrice.ToString("0.##", AU);
        ImageBox.Text = ImagePath ?? "";
        NameBox.Focus();
      };
    }

    private void Browse_Click(object sender, RoutedEventArgs e)
    {
      var dlg = new OpenFileDialog
      {
        Title = "Choose image",
        Filter = "Images|*.png;*.jpg;*.jpeg;*.gif;*.bmp|All files|*.*"
      };
      if (dlg.ShowDialog() == true) ImageBox.Text = dlg.FileName;
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
      if (string.IsNullOrWhiteSpace(NameBox.Text))
      {
        MessageBox.Show(this, "Enter a name."); return;
      }
      if (!double.TryParse(PriceBox.Text, NumberStyles.Any, AU, out var price) || price < 0)
      {
        MessageBox.Show(this, "Enter a valid price."); return;
      }
      ProductName = NameBox.Text.Trim();
      BasePrice = price;
      ImagePath = string.IsNullOrWhiteSpace(ImageBox.Text) ? null : ImageBox.Text.Trim();
      DialogResult = true;
    }

    private void Cancel_Click(object sender, RoutedEventArgs e) => DialogResult = false;
  }
}
