// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Windows;
using Maba.Gui.Services;
using Maba.Gui.ViewModels;
using Maba.Gui.Views;

namespace Maba.Gui;

public partial class App : Application
{
    private MainViewModel? _main;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        var settings = SettingsStore.Load();
        _main = new MainViewModel(settings, new SettingsStore(), Dispatcher);
        var window = new MainWindow { DataContext = _main };
        MainWindow = window;
        window.Show();
        _main.Initialize();
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _main?.Dispose();
        base.OnExit(e);
    }
}
