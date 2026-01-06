using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace pokersoc_connect.Views
{
  public partial class CashOutBreakdownView : UserControl
  {
    public event EventHandler? Back;

    public CashOutBreakdownView(string title,
                                string? subtitle,
                                IEnumerable<(int denom, int qty)> chipsRows,
                                IEnumerable<(int denom, int qty)> cashPaidOutRows,
                                IEnumerable<(int denom, int qty)> extraCashRows,
                                double tipAmount,
                                double foodTotal,
                                string? dateTime = null,
                                string? additionalInfo = null)
    {
      InitializeComponent();
      DataContext = BuildVm(title, subtitle, chipsRows, cashPaidOutRows, extraCashRows, tipAmount, foodTotal, dateTime, additionalInfo);
    }

    private object BuildVm(string title,
                           string? subtitle,
                           IEnumerable<(int denom, int qty)> chipsRows,
                           IEnumerable<(int denom, int qty)> cashPaidOutRows,
                           IEnumerable<(int denom, int qty)> extraCashRows,
                           double tipAmount,
                           double foodTotal,
                           string? dateTime,
                           string? additionalInfo)
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
          Value        = ((x.denom * x.qty) / 100.0).ToString("C", au)
        })
        .ToList();

      var cashPaidOutList = (cashPaidOutRows ?? Enumerable.Empty<(int,int)>())
        .Where(x => x.qty != 0)
        .OrderByDescending(x => x.denom)
        .Select(x => new
        {
          Denomination = CashLabel(x.denom),
          Qty          = x.qty,
          Value        = ((x.denom * x.qty) / 100.0).ToString("C", au)
        })
        .ToList();

      var extraCashList = (extraCashRows ?? Enumerable.Empty<(int,int)>())
        .Where(x => x.qty != 0)
        .OrderByDescending(x => x.denom)
        .Select(x => new
        {
          Denomination = CashLabel(x.denom),
          Qty          = x.qty,
          Value        = ((x.denom * x.qty) / 100.0).ToString("C", au)
        })
        .ToList();

      double chipsValue = chipList.Sum(r => MoneyCents(r.Value, au));
      double extraCashValue = extraCashList.Sum(r => MoneyCents(r.Value, au));
      double cashPaidOutValue = cashPaidOutList.Sum(r => MoneyCents(r.Value, au));
      double takeTotal = chipsValue + extraCashValue;
      double giveTotal = cashPaidOutValue + foodTotal + tipAmount;

      string foodDisplay = foodTotal.ToString("C", au);
      System.Diagnostics.Debug.WriteLine($"Building CashOutBreakdownView: foodTotal={foodTotal}, HasFood will be {foodTotal > 0}");
      
      if (!string.IsNullOrEmpty(additionalInfo))
      {
        System.Diagnostics.Debug.WriteLine($"Additional Info in view: '{additionalInfo}'");
        
        // Try to extract food items from additionalInfo
        // Format: "... | Food ($5.00): Coke x2, Pizza | ..."
        // We want to capture everything between "Food (...): " and the next " | " or end of string
        var foodMatch = System.Text.RegularExpressions.Regex.Match(additionalInfo, @"Food\s*\([^\)]+\):\s*([^|]+)");
        if (foodMatch.Success)
        {
          var items = foodMatch.Groups[1].Value.Trim();
          if (!string.IsNullOrWhiteSpace(items))
          {
            foodDisplay = $"{foodTotal.ToString("C", au)}: {items}";
            System.Diagnostics.Debug.WriteLine($"Food items found: '{items}'");
          }
        }
        else
        {
          System.Diagnostics.Debug.WriteLine("No food items match found in additionalInfo");
          // Try alternative format: "Food: $5.00"
          var altMatch = System.Text.RegularExpressions.Regex.Match(additionalInfo, @"Food:\s*\$[\d,]+\.?\d*");
          if (altMatch.Success)
          {
            System.Diagnostics.Debug.WriteLine("Found Food: $X.XX format (no items)");
          }
        }
      }
      else
      {
        System.Diagnostics.Debug.WriteLine("additionalInfo is null or empty in view");
      }
      
      // Always show at least the total if we have food but no items
      if (foodTotal > 0 && foodDisplay == foodTotal.ToString("C", au))
      {
        // No items were extracted, just show the total
        System.Diagnostics.Debug.WriteLine("No food items extracted, showing just total");
      }

      return new
      {
        Title            = title,
        Subtitle         = subtitle ?? "",
        DateTime         = dateTime ?? "",
        HasDateTime      = !string.IsNullOrEmpty(dateTime),
        ChipsRows        = chipList,
        ChipsTotal       = "Chips Sum: " + chipsValue.ToString("C", au),
        ExtraCashRows    = extraCashList,
        HasChips         = chipList.Count > 0,
        HasExtraCash     = extraCashList.Count > 0,
        TakeTotal        = "Total Taken: " + takeTotal.ToString("C", au),
        CashPaidOutRows  = cashPaidOutList,
        HasCashPaidOut   = cashPaidOutList.Count > 0,
        FoodDisplay      = foodDisplay,
        HasFood          = foodTotal > 0,
        TipDisplay       = tipAmount.ToString("C", au),
        HasTips          = tipAmount > 0,
        GiveTotal        = "Total Given: " + giveTotal.ToString("C", au)
      };
    }

    private static double MoneyCents(string money, CultureInfo au)
    {
      if (double.TryParse(money, NumberStyles.Currency, au, out var v)) return v;
      return 0;
    }

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
      1000 => "$10", 2000 => "$20", 5000 => "$50",
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
