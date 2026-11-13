// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Maba.Gui.Services;

public sealed record ProgramPart(string Name, string Version, string File, long Size, string Sha256);

// A newer program release and the parts of it this installation needs.
public sealed record ProgramUpdate(string Version, string Source, IReadOnlyList<ProgramPart> Parts)
{
    public long Bytes => Parts.Sum(p => p.Size);
}

// The result note the updater left for this start.
public sealed record UpdateStatus(bool Ok, string Version, string Error);

// The program updates itself from its own releases (docs/notes/updates.md). A release carries
// update.json and one package per part: "app" (the executable, bin/, docs, tools) and "ocr"
// (Python with the OCR packages and models). Only parts whose version differs are downloaded.
// The files are replaced by bin/maba_updater.exe after this program has exited: a running program
// cannot replace itself. Resources under data/ are not touched here (ResourceUpdater).
public static class ProgramUpdater
{
    private const string ManifestName = "update.json";
    private const long MaxPartBytes = 2L * 1024 * 1024 * 1024;
    private static readonly string[] AppFiles = { "LICENSE", "THIRD_PARTY_NOTICES.md", "README.md", "README.en.md" };

    private static string Root => AppContext.BaseDirectory.TrimEnd('\\', '/');
    private static string Work => Path.Combine(Root, "update");
    private static string Staging => Path.Combine(Work, "program-new");
    private static string StatusPath => Path.Combine(Work, "status.json");
    private static string ExeName => Path.GetFileName(Environment.ProcessPath ?? "MabaAssistantBlueArchive.exe");

    // A folder holding update.json and its packages replaces GitHub: offline installs and tests.
    private static string LocalFeed => Environment.GetEnvironmentVariable("MABA_UPDATE_FEED") ?? "";

    // Only an unpacked release can update itself: it has the updater in bin/ and its data beside it.
    public static bool Supported =>
        (UpdateChecker.Repository.Length > 0 || LocalFeed.Length > 0) &&
        File.Exists(Path.Combine(Root, "bin", "maba_updater.exe")) && Directory.Exists(Path.Combine(Root, "data"));

    public static async Task<ProgramUpdate?> CheckAsync()
    {
        if (!Supported || UpdateChecker.ParseVersion(UpdateChecker.CurrentVersion) is not { } current) return null;
        try
        {
            // "latest" is the newest full release: pre-releases and resource releases are never offered.
            var source = LocalFeed.Length > 0
                ? LocalFeed
                : $"https://github.com/{UpdateChecker.Repository}/releases/latest/download/";
            if (await UpdateFiles.ReadTextAsync(source, ManifestName) is not { } text) return null;
            var m = JsonNode.Parse(text)?.AsObject();
            var version = m?["version"]?.GetValue<string>() ?? "";
            if (UpdateChecker.ParseVersion(version) is not { } offered || offered <= current) return null;  // never back
            var parts = new List<ProgramPart>();
            foreach (var node in m?["parts"]?.AsArray() ?? new JsonArray())
            {
                var part = new ProgramPart(node?["name"]?.GetValue<string>() ?? "", node?["version"]?.GetValue<string>() ?? "",
                                           node?["file"]?.GetValue<string>() ?? "", node?["size"]?.GetValue<long>() ?? 0,
                                           node?["sha256"]?.GetValue<string>() ?? "");
                if (part.Name is not ("app" or "ocr") || part.File != Path.GetFileName(part.File) ||
                    !part.File.EndsWith(".zip", StringComparison.OrdinalIgnoreCase) || part.Sha256.Length != 64 ||
                    part.Size <= 0 || part.Size > MaxPartBytes)
                    return null;
                if (part.Name == "ocr" && part.Version == InstalledOcrVersion()) continue;  // unchanged
                parts.Add(part);
            }
            return parts.Any(p => p.Name == "app") ? new ProgramUpdate(version, source, parts) : null;
        }
        catch (Exception e) when (e is HttpRequestException or TaskCanceledException or JsonException or IOException
                                      or InvalidOperationException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    // Downloads, checks and unpacks the parts beside the program. "" when everything is ready.
    public static async Task<string> DownloadAsync(ProgramUpdate update)
    {
        try
        {
            UpdateFiles.DeleteDirectory(Staging);
            foreach (var part in update.Parts)
            {
                var pack = Path.Combine(Work, part.File);
                try
                {
                    var error = await UpdateFiles.FetchAsync(update.Source, part.File, pack, part.Size, part.Sha256);
                    if (error.Length > 0) return part.Name + ": " + error;
                    UpdateFiles.Extract(pack, Staging, parts => Allowed(part.Name, parts));
                }
                finally
                {
                    if (File.Exists(pack)) File.Delete(pack);
                }
            }
            return Entries().Count > 0 ? "" : "the update holds no files";
        }
        catch (Exception e) when (e is HttpRequestException or TaskCanceledException or IOException
                                      or UnauthorizedAccessException or InvalidDataException)
        {
            try
            {
                UpdateFiles.DeleteDirectory(Staging);
            }
            catch (Exception x) when (x is IOException or UnauthorizedAccessException)
            {
                // Removed by the next download.
            }
            return e.Message;
        }
    }

    public static bool Downloaded => Directory.Exists(Staging) && Entries().Count > 0;

    // Starts the updater on what DownloadAsync prepared. "" when it runs: the caller then exits at
    // once, because the updater waits for this process before it touches a file.
    public static string Launch(ProgramUpdate update)
    {
        try
        {
            var entries = Entries();
            if (entries.Count == 0) return "nothing to install";
            // A copy of the updater runs, because bin/ is among what gets replaced. The new
            // release's updater is used when it brings one.
            var fresh = Path.Combine(Staging, "bin", "maba_updater.exe");
            var updater = Path.Combine(Work, "maba_updater.exe");
            File.Copy(File.Exists(fresh) ? fresh : Path.Combine(Root, "bin", "maba_updater.exe"), updater, overwrite: true);
            if (File.Exists(StatusPath)) File.Delete(StatusPath);
            var plan = new JsonObject
            {
                ["root"] = Root,
                ["staging"] = Staging,
                ["backup"] = Path.Combine(Work, "program-old"),
                ["entries"] = new JsonArray(entries.Select(e => (JsonNode?)JsonValue.Create(e)).ToArray()),
                ["relaunch"] = ExeName,
                ["pid"] = Environment.ProcessId,
                ["status"] = StatusPath,
                ["version"] = update.Version,
            };
            var planPath = Path.Combine(Work, "plan.json");
            File.WriteAllText(planPath, plan.ToJsonString());
            var info = new ProcessStartInfo(updater) { UseShellExecute = false, WorkingDirectory = Work };
            info.ArgumentList.Add(planPath);
            using var process = Process.Start(info);
            return process is null ? "the updater did not start" : "";
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or System.ComponentModel.Win32Exception
                                      or InvalidOperationException)
        {
            return e.Message;
        }
    }

    // Reads and removes the updater's note, once per start. After a success the unpacked files
    // and the updater's copy are gone already; the replaced files stay in update/program-old.
    public static UpdateStatus? TakeStatus()
    {
        try
        {
            if (!File.Exists(StatusPath)) return null;
            var node = JsonNode.Parse(File.ReadAllText(StatusPath));
            File.Delete(StatusPath);
            return new UpdateStatus(node?["ok"]?.GetValue<bool>() ?? false, node?["version"]?.GetValue<string>() ?? "",
                                    node?["error"]?.GetValue<string>() ?? "");
        }
        catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException or InvalidOperationException)
        {
            return null;
        }
    }

    private static string InstalledOcrVersion()
    {
        try
        {
            return JsonNode.Parse(File.ReadAllText(Path.Combine(Root, "ocr", "part.json")))?["version"]?.GetValue<string>() ?? "";
        }
        catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException or InvalidOperationException)
        {
            return "";
        }
    }

    // Where a part may put files: the program part the executable, bin/, docs/, data/tools/ and
    // the licence texts; the OCR part ocr/. Never the resources or the player's own templates.
    private static bool Allowed(string part, string[] path)
    {
        if (part == "ocr") return path.Length >= 2 && path[0] == "ocr";
        if (path.Length == 1) return path[0].EndsWith(".exe", StringComparison.OrdinalIgnoreCase) || AppFiles.Contains(path[0]);
        return path[0] is "bin" or "docs" || (path.Length >= 3 && path[0] == "data" && path[1] == "tools");
    }

    // What the updater swaps: top-level files and folders of the unpacked update, and data/tools
    // instead of data (the rest of data/ belongs to the resources).
    private static List<string> Entries()
    {
        var entries = new List<string>();
        if (!Directory.Exists(Staging)) return entries;
        foreach (var path in Directory.EnumerateFileSystemEntries(Staging))
        {
            var name = Path.GetFileName(path);
            if (name == "data")
            {
                if (Directory.Exists(Path.Combine(path, "tools"))) entries.Add("data/tools");
            }
            else
            {
                entries.Add(name);
            }
        }
        entries.Sort(StringComparer.Ordinal);
        return entries;
    }
}
