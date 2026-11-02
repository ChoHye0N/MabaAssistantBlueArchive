// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Windows;

namespace Maba.Gui;

// UI text lookup from the merged Resources/Strings.<lang>.xaml dictionary.
// A missing key shows the key itself, so gaps are visible instead of silently blank.
public static class Strings
{
    public static string Get(string key) =>
        Application.Current?.TryFindResource(key) as string ?? key;

    public static string Format(string key, params object[] args) => string.Format(Get(key), args);
}
