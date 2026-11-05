// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Windows;

namespace Maba.Gui.Views;

// A question in the window's own style. The library's message box plays a system sound on opening.
public partial class ConfirmDialog
{
    private ConfirmDialog(string title, string message, string confirm, string cancel)
    {
        InitializeComponent();
        Title = title;
        MessageText.Text = message;
        ConfirmButton.Content = confirm;
        CancelButton.Content = cancel;
        Owner = Application.Current.MainWindow;
        Loaded += (_, _) => CancelButton.Focus();
    }

    public static bool Ask(string title, string message, string confirm, string cancel) =>
        new ConfirmDialog(title, message, confirm, cancel).ShowDialog() == true;

    private void OnConfirm(object sender, RoutedEventArgs e) => DialogResult = true;
}
