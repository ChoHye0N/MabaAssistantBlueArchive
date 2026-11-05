// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Globalization;
using System.IO;
using System.Text.Json.Nodes;

namespace Maba.Gui.ViewModels;

// One account on the "My info" tab, from accounts/<uid>.json written by the account_info task.
public sealed record AccountCard(string Uid, string Nickname, string Level, string Ap, string Credits, string Pyroxene, string Updated)
{
    private const string Unknown = "-";

    public static List<AccountCard> LoadAll(string folder)
    {
        var cards = new List<AccountCard>();
        if (!Directory.Exists(folder)) return cards;
        foreach (var file in Directory.GetFiles(folder, "*.json"))
        {
            if (Load(file) is { } card) cards.Add(card);
        }
        cards.Sort((a, b) => string.CompareOrdinal(a.Uid, b.Uid));
        return cards;
    }

    private static AccountCard? Load(string file)
    {
        try
        {
            var record = JsonNode.Parse(File.ReadAllText(file))?.AsObject();
            var values = record?["values"]?.AsObject();
            if (values is null) return null;
            var ap = Number(values, "ap");
            var apMax = Number(values, "ap_max");
            return new AccountCard(
                Path.GetFileNameWithoutExtension(file),
                values["nickname"]?.GetValue<string>() ?? Unknown,
                Grouped(Number(values, "level")),
                ap is null ? Unknown : apMax is null ? $"{ap}" : $"{ap} / {apMax}",
                Grouped(Number(values, "credits")),
                Grouped(Number(values, "pyroxene")),
                LocalTime(record?["updated"]?.GetValue<string>()));
        }
        catch (Exception e) when (e is IOException or System.Text.Json.JsonException or InvalidOperationException or UnauthorizedAccessException)
        {
            return null;  // a damaged record is rewritten by the next run
        }
    }

    private static long? Number(JsonObject values, string key) => values[key]?.GetValue<long>();

    private static string Grouped(long? value) =>
        value is null ? Unknown : value.Value.ToString("N0", CultureInfo.InvariantCulture);

    private static string LocalTime(string? utc) =>
        DateTimeOffset.TryParse(utc, CultureInfo.InvariantCulture, DateTimeStyles.AssumeUniversal, out var t)
            ? t.ToLocalTime().ToString("yyyy-MM-dd HH:mm", CultureInfo.InvariantCulture)
            : Unknown;
}
