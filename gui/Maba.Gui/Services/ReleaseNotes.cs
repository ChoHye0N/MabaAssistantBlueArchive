// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.IO;
using System.Net.Http;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Maba.Gui.Services;

// One section of a release's notes: its heading and its lines. `Bullet` lines were list items.
public sealed record NoteLine(string Text, bool Bullet);
public sealed record NoteSection(string Title, IReadOnlyList<NoteLine> Lines);
public sealed record ReleaseNotesInfo(string Version, string Url, IReadOnlyList<NoteSection> Sections);

// What a new version brought, shown once after the program updated itself. The text is the
// release's own notes; only what is new and what was fixed is kept. Anything else stays on the
// release page, which the window links to.
public static class ReleaseNotes
{
    // Headings of the sections that are shown, in the languages the notes are written in.
    private static readonly string[] Shown =
    {
        "새 기능", "수정",
        "New", "Fixed",
    };

    private static string LocalFeed => Environment.GetEnvironmentVariable("MABA_UPDATE_FEED") ?? "";

    // The notes of release `version`, or null when they cannot be read or hold none of the sections.
    public static async Task<ReleaseNotesInfo?> FetchAsync(string version)
    {
        if (UpdateChecker.ParseVersion(version) is null) return null;
        try
        {
            string body;
            var url = "";
            if (LocalFeed.Length > 0)
            {
                // A feed folder may carry "notes-<version>.md": offline installs and tests.
                var path = Path.Combine(LocalFeed, $"notes-{version}.md");
                if (!File.Exists(path)) return null;
                body = await File.ReadAllTextAsync(path);
            }
            else
            {
                if (UpdateChecker.Repository.Length == 0) return null;
                using var client = UpdateFiles.NewClient();
                client.Timeout = TimeSpan.FromSeconds(10);
                var json = await client.GetStringAsync(
                    $"https://api.github.com/repos/{UpdateChecker.Repository}/releases/tags/v{version}");
                var release = JsonNode.Parse(json)?.AsObject();
                body = release?["body"]?.GetValue<string>() ?? "";
                var page = release?["html_url"]?.GetValue<string>() ?? "";
                if (page.StartsWith("https://github.com/", StringComparison.Ordinal)) url = page;
            }
            var sections = Parse(body);
            return sections.Count > 0 ? new ReleaseNotesInfo(version, url, sections) : null;
        }
        catch (Exception e) when (e is HttpRequestException or TaskCanceledException or JsonException or IOException
                                      or InvalidOperationException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    // Splits notes written as "## heading" sections and keeps the shown ones with their lines.
    public static List<NoteSection> Parse(string markdown)
    {
        var sections = new List<NoteSection>();
        List<NoteLine>? lines = null;
        foreach (var raw in markdown.Replace("\r\n", "\n").Split('\n'))
        {
            var line = raw.Trim();
            if (line.StartsWith("## ", StringComparison.Ordinal))
            {
                var title = line[3..].Trim();
                lines = null;
                if (Array.IndexOf(Shown, title) >= 0)
                {
                    lines = new List<NoteLine>();
                    sections.Add(new NoteSection(title, lines));
                }
                continue;
            }
            if (lines is null || line.Length == 0 || line.StartsWith('#')) continue;
            var bullet = line.StartsWith("- ", StringComparison.Ordinal) || line.StartsWith("* ", StringComparison.Ordinal);
            lines.Add(new NoteLine(bullet ? line[2..].Trim() : line, bullet));
        }
        sections.RemoveAll(s => s.Lines.Count == 0);
        return sections;
    }

    // "**bold** and `code`" as pieces: (text, bold). Code marks are dropped, the text stays.
    public static List<(string Text, bool Bold)> Inline(string text)
    {
        var parts = new List<(string, bool)>();
        text = text.Replace("`", "");
        var bold = false;
        var start = 0;
        while (true)
        {
            var mark = text.IndexOf("**", start, StringComparison.Ordinal);
            var piece = mark < 0 ? text[start..] : text[start..mark];
            if (piece.Length > 0) parts.Add((piece, bold));
            if (mark < 0) break;
            bold = !bold;
            start = mark + 2;
        }
        return parts;
    }
}
