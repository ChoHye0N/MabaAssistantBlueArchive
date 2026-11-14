// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Diagnostics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using Maba.Gui.Services;

namespace Maba.Gui.Views;

// What the new version brought: the version, then the kept sections of the release's notes.
public partial class UpdateNotesDialog
{
    private readonly string _url;

    private UpdateNotesDialog(ReleaseNotesInfo notes, string previous)
    {
        InitializeComponent();
        Owner = Application.Current.MainWindow;
        _url = notes.Url;
        PageButton.Visibility = _url.Length > 0 ? Visibility.Visible : Visibility.Collapsed;
        VersionText.Text = previous.Length > 0 && previous != notes.Version
            ? Strings.Format("Notes.Version", previous, notes.Version)
            : Strings.Format("Notes.VersionOnly", notes.Version);
        foreach (var section in notes.Sections)
        {
            Body.Children.Add(new TextBlock
            {
                Text = section.Title, FontSize = 14, FontWeight = FontWeights.SemiBold, Margin = new Thickness(0, 10, 0, 6),
            });
            foreach (var line in section.Lines)
            {
                var text = new TextBlock { TextWrapping = TextWrapping.Wrap, LineHeight = 20 };
                foreach (var (piece, bold) in ReleaseNotes.Inline(line.Text))
                    text.Inlines.Add(new Run(piece) { FontWeight = bold ? FontWeights.SemiBold : FontWeights.Normal });
                if (!line.Bullet)
                {
                    text.Margin = new Thickness(0, 0, 0, 6);
                    Body.Children.Add(text);
                    continue;
                }
                var row = new DockPanel { Margin = new Thickness(4, 0, 0, 6) };
                var dot = new TextBlock { Text = "•", Width = 14, LineHeight = 20 };
                DockPanel.SetDock(dot, Dock.Left);
                row.Children.Add(dot);
                row.Children.Add(text);
                Body.Children.Add(row);
            }
        }
    }

    public static void Show(ReleaseNotesInfo notes, string previous) => new UpdateNotesDialog(notes, previous).ShowDialog();

    private void OnOpenPage(object sender, RoutedEventArgs e)
    {
        if (_url.Length > 0) Process.Start(new ProcessStartInfo(_url) { UseShellExecute = true });
    }
}
