using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace pokersoc_connect.Views
{
  public partial class BreakdownView : UserControl
  {
    public event EventHandler? Back;

    public BreakdownView(string title,
                         string? subtitle,
                         IEnumerable<(int denom, int qty)> chipsRows,
                         IEnumerable<(int denom, int qty)> cashRows,
                         bool isCashOut)
    {
      InitializeComponent();
      DataContext = BuildVm(title, subtitle, chipsRows, cashRows, Enumerable.Empty<(int,int)>(), isCashOut);
    }

    public BreakdownView(string title,
                         string? subtitle,
                         IEnumerable<(int denom, int qty)> chipsRows,
                         IEnumerable<(int denom, int qty)> cashRows,
                         IEnumerable<(int denom, int qty)> changeRows,
                         bool isCashOut)
    {
      InitializeComponent();
      DataContext = BuildVm(title, subtitle, chipsRows, cashRows, changeRows, isCashOut);
    }

    private object BuildVm(string title,
                           string? subtitle,
                           IEnumerable<(int denom, int qty)> chipsRows,
                           IEnumerable<(int denom, int qty)> cashRows,
                           IEnumerable<(int denom, int qty)> changeRows,
                           bool isCashOut)
    {
      var au = CultureInfo.GetCultureInfo("en-AU");

      var chipList = (chipsRows ?? Enumerable.Empty<(int,int)>())
        .Where(x => x.qty != 0)
        .OrderByDescending(x => x.denom)
        .Select(x => new
        {
          Denomination = DenomLabel(x.denom),
          Colour       = ChipColor(x.denom),
          Qty          = x.qty,
          Value        = ((x.denom * Math.Abs(x.qty)) / 100.0).ToString("C", au)
        })
        .ToList();

      var cashList = (cashRows ?? Enumerable.Empty<(int,int)>())
        .Where(x => x.qty != 0)
        .OrderByDescending(x => x.denom)
        .Select(x => new
        {
          Denomination = CashLabel(x.denom),
          Qty          = Math.Abs(x.qty),
          Value        = ((x.denom * Math.Abs(x.qty)) / 100.0).ToString("C", au)
        })
        .ToList();

      var changeList = (changeRows ?? Enumerable.Empty<(int,int)>())
        .Where(x => x.qty != 0)
        .OrderByDescending(x => x.denom)
        .Select(x => new
        {
          Denomination = CashLabel(x.denom),
          Qty          = x.qty,
          Value        = ((x.denom * x.qty) / 100.0).ToString("C", au)
        })
        .ToList();

      double total = chipList.Sum(r => MoneyCents(r.Value, au)) + cashList.Sum(r => MoneyCents(r.Value, au));
      var grand = $"Total: {total.ToString("C", au)}";

      return new
      {
        Title        = title,
        Subtitle     = subtitle ?? "",
        ChipsRows    = chipList,
        CashRows     = cashList,
        ChangeRows   = changeList,
        CashHeader   = isCashOut ? "Cash paid out" : "Cash received",
        ChangeHeader = "Change given back",
        HasChips     = chipList.Count > 0,
        HasCash      = cashList.Count > 0,
        HasChange    = changeList.Count > 0,
        GrandTotal   = grand
      };
    }

    private static double MoneyCents(string money, CultureInfo au)
    {
      if (double.TryParse(money, NumberStyles.Currency, au, out var v)) return v;
      return 0;
    }

    // Labels…
    private static string DenomLabel(int cents) => cents switch
    {
      5 => "5c",
      25 => "25c",
      100 => "$1",
      200 => "$2",
      500 => "$5",
      2500 => "$25",
      10000 => "$100",
      _ => $"{cents}c"
    };

    private static string CashLabel(int cents) => cents switch
    {
      5 => "5c", 10 => "10c", 20 => "20c", 50 => "50c",
      100 => "$1", 200 => "$2", 500 => "$5",
      2500 => "$25", 10000 => "$100",
      _ => $"{cents}c"
    };

    private static string ChipColor(int cents) => cents switch
    {
      5     => "White (5c)",
      25    => "Red (25c)",
      100   => "Blue ($1)",
      200   => "Green ($2)",
      500   => "Black ($5)",
      2500  => "White Plaque ($25)",
      10000 => "Red Plaque ($100)",
      _     => "Custom"
    };

    private void Back_Click(object sender, RoutedEventArgs e) => Back?.Invoke(this, EventArgs.Empty);
  }
}
