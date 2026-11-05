// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Net.Http;
using System.Reflection;
using System.Text.Json.Nodes;

namespace Maba.Gui.Services;

public sealed record UpdateInfo(string Version, string Url);

// Asks GitHub for the latest release. It only tells: nothing is downloaded or installed, and no
// game or account data is sent. A failed check stays silent, because it must never disturb a run.
public static class UpdateChecker
{
    private static readonly TimeSpan Timeout = TimeSpan.FromSeconds(5);

    // "owner/name", set by the release build; other builds have none and never check.
    private static string Repository =>
        Assembly.GetExecutingAssembly().GetCustomAttributes<AssemblyMetadataAttribute>()
            .FirstOrDefault(a => a.Key == "Repository")?.Value ?? "";

    private static string CurrentVersion =>
        Assembly.GetExecutingAssembly().GetCustomAttribute<AssemblyInformationalVersionAttribute>()
            ?.InformationalVersion ?? "";

    public static async Task<UpdateInfo?> CheckAsync()
    {
        if (string.IsNullOrEmpty(Repository) || ParseVersion(CurrentVersion) is not { } current) return null;
        try
        {
            using var client = new HttpClient { Timeout = Timeout };
            client.DefaultRequestHeaders.UserAgent.ParseAdd("MabaAssistantBlueArchive");
            var body = await client.GetStringAsync($"https://api.github.com/repos/{Repository}/releases/latest");
            var release = JsonNode.Parse(body)?.AsObject();
            var tag = release?["tag_name"]?.GetValue<string>();
            var url = release?["html_url"]?.GetValue<string>();
            if (tag is null || url is null || !url.StartsWith("https://github.com/", StringComparison.Ordinal)) return null;
            return ParseVersion(tag) is { } latest && latest > current ? new UpdateInfo(tag.TrimStart('v'), url) : null;
        }
        catch (Exception e) when (e is HttpRequestException or TaskCanceledException or System.Text.Json.JsonException
                                      or InvalidOperationException)
        {
            return null;
        }
    }

    // "v0.1.0", "0.1.0-beta.2" and "0.1.0+abc" all give 0.1.0.
    private static Version? ParseVersion(string text)
    {
        var plain = text.TrimStart('v').Split('-', '+')[0];
        return Version.TryParse(plain, out var version) ? version : null;
    }
}
