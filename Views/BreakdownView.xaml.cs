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
    public event EventHandler<TransactionDeletedEventArgs>? Deleted;

    private long? _txId;
    private string? _txType;
    private string? _batchId;
    private string _enteredPassword = "";

    public BreakdownView(string title,
                         string? subtitle,
                         IEnumerable<(int denom, int qty)> chipsRows,
                         IEnumerable<(int denom, int qty)> cashRows,
                         bool isCashOut,
                         string? dateTime = null,
                         string? additionalInfo = null)
    {
      InitializeComponent();
      DataContext = BuildVm(title, subtitle, chipsRows, cashRows, Enumerable.Empty<(int,int)>(), isCashOut, dateTime, additionalInfo);
    }

    public BreakdownView(string title,
                         string? subtitle,
                         IEnumerable<(int denom, int qty)> chipsRows,
                         IEnumerable<(int denom, int qty)> cashRows,
                         IEnumerable<(int denom, int qty)> changeRows,
                         bool isCashOut,
                         string? dateTime = null,
                         string? additionalInfo = null)
    {
      InitializeComponent();
      DataContext = BuildVm(title, subtitle, chipsRows, cashRows, changeRows, isCashOut, dateTime, additionalInfo);
    }

    // Enable deletion for transactions
    public void EnableDeletion(long txId, string txType)
    {
      _txId = txId;
      _txType = txType;
      DeleteButton.Visibility = Visibility.Visible;
    }

    // Enable deletion for float additions
    public void EnableDeletionForFloat(string batchId)
    {
      _batchId = batchId;
      _txType = "FLOAT";
      DeleteButton.Visibility = Visibility.Visible;
    }

    // Enable deletion for lost chips
    public void EnableDeletionForLostChips(string batchId)
    {
      _batchId = batchId;
      _txType = "LOST_CHIP";
      DeleteButton.Visibility = Visibility.Visible;
    }

    private void Delete_Click(object sender, RoutedEventArgs e)
    {
      _enteredPassword = "";
      UpdatePasswordDisplay();
      PasswordErrorText.Visibility = Visibility.Collapsed;
      PasswordPanel.Visibility = Visibility.Visible;
    }

    private void Keypad_Click(object sender, RoutedEventArgs e)
    {
      if (sender is Button button && button.Tag is string digit)
      {
        _enteredPassword += digit;
        UpdatePasswordDisplay();
        PasswordErrorText.Visibility = Visibility.Collapsed;
      }
    }

    private void KeypadBackspace_Click(object sender, RoutedEventArgs e)
    {
      if (_enteredPassword.Length > 0)
      {
        _enteredPassword = _enteredPassword.Substring(0, _enteredPassword.Length - 1);
        UpdatePasswordDisplay();
      }
    }

    private void KeypadClear_Click(object sender, RoutedEventArgs e)
    {
      _enteredPassword = "";
      UpdatePasswordDisplay();
      PasswordErrorText.Visibility = Visibility.Collapsed;
    }

    private void UpdatePasswordDisplay()
    {
      PasswordDisplay.Text = _enteredPassword.Length > 0 
        ? new string('●', _enteredPassword.Length) 
        : "****";
    }

    private void PasswordCancel_Click(object sender, RoutedEventArgs e)
    {
      PasswordPanel.Visibility = Visibility.Collapsed;
      _enteredPassword = "";
    }

    private void PasswordConfirm_Click(object sender, RoutedEventArgs e)
    {
      var correctPassword = Database.GetAdminPassword();

      if (_enteredPassword == correctPassword)
      {
        PasswordPanel.Visibility = Visibility.Collapsed;
        _enteredPassword = "";
        PerformDeletion();
      }
      else
      {
        PasswordErrorText.Visibility = Visibility.Visible;
        _enteredPassword = "";
        UpdatePasswordDisplay();
      }
    }

    private void PerformDeletion()
    {
      try
      {
        if (_txType == "FLOAT" && !string.IsNullOrEmpty(_batchId))
        {
          Database.DeleteFloatAddition(_batchId);
        }
        else if (_txType == "LOST_CHIP" && !string.IsNullOrEmpty(_batchId))
        {
          Database.DeleteLostChips(_batchId);
        }
        else if (_txId.HasValue && !string.IsNullOrEmpty(_txType))
        {
          Database.DeleteTransaction(_txId.Value, _txType);
        }
        
        Deleted?.Invoke(this, new TransactionDeletedEventArgs(_txId, _batchId, _txType));
      }
      catch (Exception ex)
      {
        MessageBox.Show($"Error deleting: {ex.Message}", "Delete Error", MessageBoxButton.OK, MessageBoxImage.Error);
      }
    }

    private object BuildVm(string title,
                           string? subtitle,
                           IEnumerable<(int denom, int qty)> chipsRows,
                           IEnumerable<(int denom, int qty)> cashRows,
                           IEnumerable<(int denom, int qty)> changeRows,
                           bool isCashOut,
                           string? dateTime = null,
                           string? additionalInfo = null)
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

      // For cashouts: only show cash paid out (not chips turned in)
      // For buy-ins: show total of cash received + change given (chips + cash)
      double total;
      if (isCashOut)
      {
        total = cashList.Sum(r => MoneyCents(r.Value, au));
      }
      else
      {
        total = chipList.Sum(r => MoneyCents(r.Value, au)) + cashList.Sum(r => MoneyCents(r.Value, au));
      }
      var grand = $"Total: {total.ToString("C", au)}";

      return new
      {
        Title        = title,
        Subtitle     = subtitle ?? "",
        DateTime     = dateTime ?? "",
        AdditionalInfo = additionalInfo ?? "",
        ChipsRows    = chipList,
        CashRows     = cashList,
        ChangeRows   = changeList,
        CashHeader   = isCashOut ? "Cash paid out" : "Cash received",
        ChangeHeader = "Change given back",
        HasChips     = chipList.Count > 0,
        HasCash      = cashList.Count > 0,
        HasChange    = changeList.Count > 0,
        HasDateTime  = !string.IsNullOrEmpty(dateTime),
        HasAdditionalInfo = !string.IsNullOrEmpty(additionalInfo),
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

  public class TransactionDeletedEventArgs : EventArgs
  {
    public long? TxId { get; }
    public string? BatchId { get; }
    public string? TxType { get; }

    public TransactionDeletedEventArgs(long? txId, string? batchId, string? txType)
    {
      TxId = txId;
      BatchId = batchId;
      TxType = txType;
    }
  }
}
