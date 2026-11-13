// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.IO;
using System.Net.Http;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Maba.Gui.Services;

// What a resource release offers. NeedsProgram: the pack needs a newer program than this one.
public sealed record ResourceUpdate(string Version, string MinCore, string File, long Size, string Sha256,
                                    string Source, bool NeedsProgram);

// Resources (tasks, pages, popups, strings, config, templates) are updated apart from the program,
// from releases of the program's own repository tagged "resources-<version>". Behaviour notes:
// docs/notes/updates.md.
//
// Nothing is applied unless: the pack is newer than the installed one, this program is new enough
// for it, size and SHA-256 match the release's manifest, every entry stays inside the resource
// folders, and Core loads the new data without an error. The folders are then swapped and the
// previous ones kept, so a failed swap is undone. No game or account data is sent.
public static class ResourceUpdater
{
    public const string TagPrefix = "resources-";
    private const string ManifestName = "resources.json";
    private const long MaxPackBytes = 64L * 1024 * 1024;
    private static readonly string[] Folders = { "config", "pages", "popups", "tasks", "strings", "assets" };

    // A folder holding resources.json and its pack replaces GitHub: offline installs and tests.
    private static string LocalFeed => Environment.GetEnvironmentVariable("MABA_RESOURCE_FEED") ?? "";

    // Only a release keeps its data beside the executable; a development build reads the repository,
    // which is never changed from here.
    public static bool Supported(string dataRoot) =>
        (UpdateChecker.Repository.Length > 0 || LocalFeed.Length > 0) &&
        UpdateFiles.SamePath(dataRoot, Path.Combine(AppContext.BaseDirectory, "data"));

    public static string InstalledVersion(string dataRoot)
    {
        try
        {
            var node = JsonNode.Parse(File.ReadAllText(Path.Combine(dataRoot, "config", "resource.json")));
            return node?["version"]?.GetValue<string>() ?? "";
        }
        catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException or InvalidOperationException)
        {
            return "";
        }
    }

    // The newest resource release that is newer than the installed pack, or null.
    public static async Task<ResourceUpdate?> CheckAsync(string dataRoot)
    {
        if (!Supported(dataRoot)) return null;
        try
        {
            var (manifestText, source) = await ReadManifestAsync();
            if (manifestText is null) return null;
            var m = JsonNode.Parse(manifestText)?.AsObject();
            var version = m?["version"]?.GetValue<string>() ?? "";
            var minCore = m?["min_core"]?.GetValue<string>() ?? "";
            var file = m?["file"]?.GetValue<string>() ?? "";
            var sha = m?["sha256"]?.GetValue<string>() ?? "";
            var size = m?["size"]?.GetValue<long>() ?? 0;
            if (!Version.TryParse(version, out var offered) || !Version.TryParse(minCore, out var needs)) return null;
            if (file != Path.GetFileName(file) || !file.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)) return null;
            if (sha.Length != 64 || size <= 0 || size > MaxPackBytes) return null;
            // Never back to an older pack.
            if (Version.TryParse(InstalledVersion(dataRoot), out var installed) && offered <= installed) return null;
            var program = UpdateChecker.ParseVersion(UpdateChecker.CurrentVersion);
            return new ResourceUpdate(version, minCore, file, size, sha.ToLowerInvariant(), source,
                                      NeedsProgram: program is null || needs > program);
        }
        catch (Exception e) when (e is HttpRequestException or TaskCanceledException or JsonException or IOException
                                      or InvalidOperationException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    // Downloads, checks and installs the pack. `validate` gets the folder with the new data and
    // returns an error text ("" = Core loaded it). Returns "" on success, else why nothing changed.
    public static async Task<string> ApplyAsync(ResourceUpdate update, string dataRoot, Func<string, string> validate)
    {
        if (update.NeedsProgram) return "needs program " + update.MinCore;
        var work = Path.Combine(Path.GetDirectoryName(dataRoot.TrimEnd('\\', '/')) ?? dataRoot, "update");
        var pack = Path.Combine(work, update.File);
        var staging = Path.Combine(work, "resources-new");
        var previous = Path.Combine(work, "resources-old");
        try
        {
            Directory.CreateDirectory(work);
            UpdateFiles.DeleteDirectory(staging);
            var problem = await UpdateFiles.FetchAsync(update.Source, update.File, pack, update.Size, update.Sha256);
            if (problem.Length > 0) return problem;
            // Every entry has to lie inside one of the resource folders (nothing in assets-local).
            UpdateFiles.Extract(pack, staging, parts => parts.Length >= 2 && Array.IndexOf(Folders, parts[0]) >= 0);
            var error = validate(staging);
            if (error.Length > 0) return "the new data does not load: " + error;

            UpdateFiles.DeleteDirectory(previous);
            Directory.CreateDirectory(previous);
            var moved = new List<string>();
            try
            {
                foreach (var folder in Folders)
                {
                    var fresh = Path.Combine(staging, folder);
                    if (!Directory.Exists(fresh)) continue;
                    var live = Path.Combine(dataRoot, folder);
                    if (Directory.Exists(live)) Directory.Move(live, Path.Combine(previous, folder));
                    moved.Add(folder);
                    Directory.Move(fresh, live);
                }
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                // Put back what was moved: the installed data stays as it was.
                foreach (var folder in moved)
                {
                    var live = Path.Combine(dataRoot, folder);
                    var old = Path.Combine(previous, folder);
                    try
                    {
                        if (!Directory.Exists(old)) continue;
                        UpdateFiles.DeleteDirectory(live);
                        Directory.Move(old, live);
                    }
                    catch (Exception x) when (x is IOException or UnauthorizedAccessException)
                    {
                        return "could not restore " + folder + ": " + x.Message;
                    }
                }
                return "could not replace the data: " + e.Message;
            }
            return "";
        }
        catch (Exception e) when (e is HttpRequestException or TaskCanceledException or IOException
                                      or UnauthorizedAccessException or InvalidDataException)
        {
            return e.Message;
        }
        finally
        {
            try
            {
                UpdateFiles.DeleteDirectory(staging);
                if (File.Exists(pack)) File.Delete(pack);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                // Left-overs are removed by the next update.
            }
        }
    }

    private static async Task<(string? text, string source)> ReadManifestAsync()
    {
        if (LocalFeed.Length > 0) return (await UpdateFiles.ReadTextAsync(LocalFeed, ManifestName), LocalFeed);
        using var client = UpdateFiles.NewClient();
        var body = await client.GetStringAsync($"https://api.github.com/repos/{UpdateChecker.Repository}/releases?per_page=30");
        Version? best = null;
        var tag = "";
        foreach (var release in JsonNode.Parse(body)?.AsArray() ?? new JsonArray())
        {
            var name = release?["tag_name"]?.GetValue<string>() ?? "";
            if (release?["draft"]?.GetValue<bool>() == true || !name.StartsWith(TagPrefix, StringComparison.Ordinal)) continue;
            if (!Version.TryParse(name[TagPrefix.Length..], out var v) || (best is not null && v <= best)) continue;
            best = v;
            tag = name;
        }
        if (best is null) return (null, "");
        var source = $"https://github.com/{UpdateChecker.Repository}/releases/download/{tag}/";
        return (await client.GetStringAsync(source + ManifestName), source);
    }
}
