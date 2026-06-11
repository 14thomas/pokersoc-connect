using System;
using System.Windows;
using System.Windows.Controls;

namespace pokersoc_connect.Views
{
  public partial class ComingSoonView : UserControl
  {
    public event EventHandler? CloseRequested;

    public ComingSoonView(string title = "Coming Soon", string message = "This feature is not available yet.")
    {
      InitializeComponent();
      TitleText.Text = title;
      MessageText.Text = message;
    }

    private void Close_Click(object sender, RoutedEventArgs e) => CloseRequested?.Invoke(this, EventArgs.Empty);
  }
}
