using System;
using System.Data;
using System.Windows;
using System.Windows.Input;

namespace pokersoc_connect
{
  public partial class MainWindow : Window
  {
    public MainWindow()
    {
      InitializeComponent();
      RefreshTransactions();
    }

    private void RefreshTransactions()
    {
      var dt = Database.Query(
        "SELECT tx_id, time, type, player_id, session_id, cash_amt, chips_amt, method, staff FROM transactions ORDER BY tx_id DESC"
      );
      TxGrid.ItemsSource = dt.DefaultView;
    }

    private void EnsurePlayer(string member)
    {
      Database.Exec("INSERT OR IGNORE INTO players(member_no, first_name, last_name, display_name) VALUES ($m,'New','Player',$m)",
                    ("$m", member));
    }

    private long LatestSessionId()
    {
      return Database.ScalarLong("SELECT session_id FROM sessions ORDER BY session_id DESC LIMIT 1");
    }

    private long PlayerIdByMember(string member)
    {
      return Database.ScalarLong("SELECT player_id FROM players WHERE member_no=$m", ("$m", member));
    }

    private void BuyIn_Click(object sender, RoutedEventArgs e)
    {
      if (Database.Conn is null) { MessageBox.Show(this, "Open a session first."); return; }
      var member = string.IsNullOrWhiteSpace(ScanBox.Text) ? "0001" : ScanBox.Text.Trim();
      EnsurePlayer(member);
      var pid = PlayerIdByMember(member);
      var sid = LatestSessionId();

      // Simple fixed values; replace with proper dialog later
      Database.Exec("INSERT INTO transactions(session_id, player_id, type, cash_amt, chips_amt, method, staff) " +
                    "VALUES ($s,$p,'BUYIN',$cash,$chips,'Cash','Dealer')",
                    ("$s", sid), ("$p", pid), ("$cash", 300.0), ("$chips", 300.0));
      RefreshTransactions();
      StatusText.Text = $"Buy-in recorded for {member}";
    }

    private void CashOut_Click(object sender, RoutedEventArgs e)
    {
      if (Database.Conn is null) { MessageBox.Show(this, "Open a session first."); return; }
      var member = string.IsNullOrWhiteSpace(ScanBox.Text) ? "0001" : ScanBox.Text.Trim();
      EnsurePlayer(member);
      var pid = PlayerIdByMember(member);
      var sid = LatestSessionId();

      Database.Exec("INSERT INTO transactions(session_id, player_id, type, cash_amt, chips_amt, method, staff) " +
                    "VALUES ($s,$p,'CASHOUT',$cash,$chips,'Cash','Dealer')",
                    ("$s", sid), ("$p", pid), ("$cash", 420.0), ("$chips", 420.0));
      RefreshTransactions();
      StatusText.Text = $"Cash-out recorded for {member}";
    }

    private void ScanBox_KeyDown(object sender, KeyEventArgs e)
    {
      if (e.Key == Key.Enter)
      {
        StatusText.Text = $"Scanned: {ScanBox.Text}";
      }
    }
  }
}
