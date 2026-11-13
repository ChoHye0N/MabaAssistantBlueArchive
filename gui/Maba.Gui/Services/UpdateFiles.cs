// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Security.Cryptography;

namespace Maba.Gui.Services;

// Downloading, checking and unpacking update packages, shared by the resource and the program
// updater. A source is either an https address ending in "/" or a local folder (tests, offline).
internal static class UpdateFiles
{
    private static readonly TimeSpan Timeout = TimeSpan.FromMinutes(10);

    public static HttpClient NewClient()
    {
        var client = new HttpClient { Timeout = Timeout };
        client.DefaultRequestHeaders.UserAgent.ParseAdd("MabaAssistantBlueArchive");
        return client;
    }

    public static bool IsRemote(string source) => source.StartsWith("https://", StringComparison.Ordinal);

    public static async Task<string?> ReadTextAsync(string source, string name)
    {
        if (!IsRemote(source))
        {
            var path = Path.Combine(source, name);
            return File.Exists(path) ? await File.ReadAllTextAsync(path) : null;
        }
        using var client = NewClient();
        using var response = await client.GetAsync(source + name);
        return response.IsSuccessStatusCode ? await response.Content.ReadAsStringAsync() : null;
    }

    // Fetches `name` into `target` and returns "" when its size and SHA-256 are the expected ones.
    public static async Task<string> FetchAsync(string source, string name, string target, long size, string sha256)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(target)!);
        if (!IsRemote(source))
        {
            File.Copy(Path.Combine(source, name), target, overwrite: true);
        }
        else
        {
            using var client = NewClient();
            using var response = await client.GetAsync(source + name, HttpCompletionOption.ResponseHeadersRead);
            response.EnsureSuccessStatusCode();
            if (response.Content.Headers.ContentLength is long length && length != size)
                return $"size {length} is not {size}";
            await using var file = File.Create(target);
            await using var stream = await response.Content.ReadAsStreamAsync();
            var buffer = new byte[81920];
            long total = 0;
            int read;
            while ((read = await stream.ReadAsync(buffer)) > 0)
            {
                total += read;
                if (total > size) return "the download is larger than the manifest says";
                await file.WriteAsync(buffer.AsMemory(0, read));
            }
        }
        var bytes = new FileInfo(target).Length;
        if (bytes != size) return $"size {bytes} is not {size}";
        await using var check = File.OpenRead(target);
        var hash = Convert.ToHexString(await SHA256.HashDataAsync(check)).ToLowerInvariant();
        return hash == sha256.ToLowerInvariant() ? "" : "checksum mismatch";
    }

    // Unpacks into `staging`. Every entry has to be a plain relative path (no absolute path, no
    // "..", no drive) that `allowed` accepts by its parts; anything else stops the unpacking.
    public static void Extract(string pack, string staging, Func<string[], bool> allowed)
    {
        Directory.CreateDirectory(staging);
        var root = Path.GetFullPath(staging) + Path.DirectorySeparatorChar;
        using var zip = ZipFile.OpenRead(pack);
        foreach (var entry in zip.Entries)
        {
            if (entry.FullName.EndsWith('/')) continue;
            var parts = entry.FullName.Split('/');
            if (entry.FullName.Contains('\\') || entry.FullName.Contains(':') ||
                parts.Any(p => p.Length == 0 || p == "." || p == "..") || !allowed(parts))
                throw new InvalidDataException("unexpected entry in the pack: " + entry.FullName);
            var target = Path.GetFullPath(Path.Combine(staging, Path.Combine(parts)));
            if (!target.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("unexpected entry in the pack: " + entry.FullName);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            entry.ExtractToFile(target, overwrite: true);
        }
    }

    public static void DeleteDirectory(string path)
    {
        if (Directory.Exists(path)) Directory.Delete(path, recursive: true);
    }

    public static bool SamePath(string a, string b) =>
        string.Equals(Path.GetFullPath(a).TrimEnd('\\', '/'), Path.GetFullPath(b).TrimEnd('\\', '/'),
                      StringComparison.OrdinalIgnoreCase);
}
