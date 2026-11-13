// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Security.Cryptography;
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
    private static readonly TimeSpan Timeout = TimeSpan.FromSeconds(30);

    // A folder holding resources.json and its pack replaces GitHub: offline installs and tests.
    private static string LocalFeed => Environment.GetEnvironmentVariable("MABA_RESOURCE_FEED") ?? "";

    // Only a release keeps its data beside the executable; a development build reads the repository,
    // which is never changed from here.
    public static bool Supported(string dataRoot) =>
        (UpdateChecker.Repository.Length > 0 || LocalFeed.Length > 0) &&
        SamePath(dataRoot, Path.Combine(AppContext.BaseDirectory, "data"));

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
            DeleteDirectory(staging);
            await DownloadAsync(update, pack);
            var bytes = new FileInfo(pack).Length;
            if (bytes != update.Size) return $"size {bytes} is not {update.Size}";
            if (await Sha256Async(pack) != update.Sha256) return "checksum mismatch";
            Extract(pack, staging);
            var error = validate(staging);
            if (error.Length > 0) return "the new data does not load: " + error;

            DeleteDirectory(previous);
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
                        DeleteDirectory(live);
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
                DeleteDirectory(staging);
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
        if (LocalFeed.Length > 0)
        {
            var path = Path.Combine(LocalFeed, ManifestName);
            return File.Exists(path) ? (await File.ReadAllTextAsync(path), LocalFeed) : (null, "");
        }
        using var client = NewClient();
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

    private static async Task DownloadAsync(ResourceUpdate update, string target)
    {
        if (!update.Source.StartsWith("https://", StringComparison.Ordinal))
        {
            File.Copy(Path.Combine(update.Source, update.File), target, overwrite: true);
            return;
        }
        using var client = NewClient();
        using var response = await client.GetAsync(update.Source + update.File, HttpCompletionOption.ResponseHeadersRead);
        response.EnsureSuccessStatusCode();
        if (response.Content.Headers.ContentLength is long length && length != update.Size)
            throw new InvalidDataException($"size {length} is not {update.Size}");
        await using var file = File.Create(target);
        await using var stream = await response.Content.ReadAsStreamAsync();
        var buffer = new byte[81920];
        long total = 0;
        int read;
        while ((read = await stream.ReadAsync(buffer)) > 0)
        {
            total += read;
            if (total > update.Size) throw new InvalidDataException("the download is larger than the manifest says");
            await file.WriteAsync(buffer.AsMemory(0, read));
        }
    }

    // Every entry has to stay inside one of the resource folders: no absolute paths, no "..",
    // nothing in assets-local or beside the folders.
    private static void Extract(string pack, string staging)
    {
        Directory.CreateDirectory(staging);
        var root = Path.GetFullPath(staging) + Path.DirectorySeparatorChar;
        using var zip = ZipFile.OpenRead(pack);
        foreach (var entry in zip.Entries)
        {
            if (entry.FullName.EndsWith('/')) continue;
            var parts = entry.FullName.Split('/');
            if (entry.FullName.Contains('\\') || entry.FullName.Contains(':') || parts.Length < 2 ||
                Array.IndexOf(Folders, parts[0]) < 0 || parts.Any(p => p.Length == 0 || p == "." || p == ".."))
                throw new InvalidDataException("unexpected entry in the pack: " + entry.FullName);
            var target = Path.GetFullPath(Path.Combine(staging, Path.Combine(parts)));
            if (!target.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("unexpected entry in the pack: " + entry.FullName);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            entry.ExtractToFile(target, overwrite: true);
        }
    }

    private static async Task<string> Sha256Async(string path)
    {
        await using var stream = File.OpenRead(path);
        return Convert.ToHexString(await SHA256.HashDataAsync(stream)).ToLowerInvariant();
    }

    private static HttpClient NewClient()
    {
        var client = new HttpClient { Timeout = Timeout };
        client.DefaultRequestHeaders.UserAgent.ParseAdd("MabaAssistantBlueArchive");
        return client;
    }

    private static void DeleteDirectory(string path)
    {
        if (Directory.Exists(path)) Directory.Delete(path, recursive: true);
    }

    private static bool SamePath(string a, string b) =>
        string.Equals(Path.GetFullPath(a).TrimEnd('\\', '/'), Path.GetFullPath(b).TrimEnd('\\', '/'),
                      StringComparison.OrdinalIgnoreCase);
}
