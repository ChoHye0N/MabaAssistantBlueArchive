// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Globalization;
using System.Windows;
using Maba.Gui.Services;
using Maba.Gui.ViewModels;
using Maba.Gui.Views;

namespace Maba.Gui;

public partial class App : Application
{
    // Display languages with a Resources/Strings.<code>.xaml file. "ko" is the baseline in App.xaml.
    public static readonly IReadOnlyList<string> UiLanguages = new[] { "ko", "en" };

    private MainViewModel? _main;

    // "" (automatic) follows the Windows display language: Korean -> ko, anything else -> en.
    public static string ResolveUiLanguage(string setting)
    {
        if (UiLanguages.Contains(setting)) return setting;
        return CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "ko" ? "ko" : "en";
    }

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        var settings = SettingsStore.Load();
        // Before any view model is built: they read their text once.
        ApplyUiLanguage(ResolveUiLanguage(settings.UiLanguage));
        _main = new MainViewModel(settings, new SettingsStore(), Dispatcher);
        var window = new MainWindow { DataContext = _main };
        MainWindow = window;
        window.Show();
        _main.Initialize();
    }

    // Merged after the Korean baseline, so its keys win and a missing key falls back to Korean.
    private void ApplyUiLanguage(string language)
    {
        if (language == "ko") return;
        Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri($"pack://application:,,,/Resources/Strings.{language}.xaml", UriKind.Absolute),
        });
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _main?.Dispose();
        base.OnExit(e);
    }
}
