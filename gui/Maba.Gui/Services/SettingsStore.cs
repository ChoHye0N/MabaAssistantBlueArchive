// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.IO;
using System.Text.Json;

namespace Maba.Gui.Services;

public sealed class AppSettings
{
    public string DataRoot { get; set; } = "";       // empty = auto-detect (folder with tasks/ and pages/)
    public string Language { get; set; } = "ko";      // game client language (templates, OCR, strings/)
    public string UiLanguage { get; set; } = "";      // display language: "" = Windows language, "ko", "en"
    public string Controller { get; set; } = "steam"; // "steam" | "adb"
    public string Capture { get; set; } = "bitblt";   // Steam: "bitblt" | "printwindow"
    public bool FitWindow { get; set; } = true;       // Steam: change a window that is not 16:9 or too small
    public string AdbPath { get; set; } = "adb";
    public string AdbSerial { get; set; } = "127.0.0.1:16384";
    public string AdbScreencap { get; set; } = "auto";
    public string AdbTouch { get; set; } = "adb";     // "adb" | "maatouch"
    public string MaaTouchPath { get; set; } = "";
    public int BackKeycode { get; set; } = 111;
    // Emulator: start it (and the game) before a run, close them after (see MabaStart "launch").
    public bool StartEmulator { get; set; }
    public string EmulatorPath { get; set; } = "";
    public string EmulatorArgs { get; set; } = "";
    public bool StartGame { get; set; }
    public bool CloseGameAfter { get; set; }
    public bool CloseEmulatorAfter { get; set; }
    // Steam client: open the game through Steam before a run, close it after.
    public bool SteamStartGame { get; set; }
    public bool SteamCloseGame { get; set; }
    // Ctrl+Alt+S stops a run from any window: the run owns the mouse, so the stop button is hard to reach.
    public bool StopHotkey { get; set; } = true;
    // Failed tasks that are safe to repeat (never a purchase) run once more at the end of a run.
    public bool RetryFailed { get; set; } = true;
    // After a run that was not stopped by hand: "none", "exit" (close this program) or "shutdown" (turn the PC off).
    public string AfterRun { get; set; } = "none";
    public bool OcrEnabled { get; set; } = true;
    public bool CheckUpdates { get; set; } = true;
    // Resources (tasks, pages, templates) newer than the installed ones are installed without asking.
    public bool AutoUpdateResources { get; set; } = true;
    // A new program version: "notify" (a link installs it), "download" (fetched at once, a link
    // installs it) or "install" (installed at start, the program starts again by itself).
    public string ProgramUpdate { get; set; } = "notify";
    // Named sets of selected tasks, their settings and their order; one is active.
    public List<TaskProfile> Profiles { get; set; } = new();
    public int ActiveProfile { get; set; }
    public bool ScheduleEnabled { get; set; }
    public int ResetHourUtc { get; set; } = 19;      // 04:00 KST; verify in the game
    public int ResetMinuteUtc { get; set; }
    public int DelayMinutes { get; set; } = 5;
    // "reset": DelayMinutes after the server reset; "clock": every day at ScheduleHour:ScheduleMinute local time.
    public string ScheduleMode { get; set; } = "reset";
    public int ScheduleHour { get; set; } = 7;
    public int ScheduleMinute { get; set; }
    public DateTimeOffset? LastScheduledRun { get; set; }
    // Last outcome per task id and of the last run, shown in the tasks tab (no account data).
    public Dictionary<string, TaskResult> LastResults { get; set; } = new();
    public RunSummary? LastRun { get; set; }
}

public sealed record TaskResult(DateTimeOffset Time, string Status, string Message);

public sealed record RunSummary(DateTimeOffset Time, int Succeeded, int Failed, int Skipped, bool Cancelled);

// A player's set of dailies: which tasks, their settings, and the order they run in.
public sealed partial class TaskProfile : CommunityToolkit.Mvvm.ComponentModel.ObservableObject
{
    [CommunityToolkit.Mvvm.ComponentModel.ObservableProperty] private string _name = "";
    public List<string> SelectedTasks { get; set; } = new();
    // Task settings by task id and key, as JSON values (see tasks/<id>.json "settings").
    public Dictionary<string, Dictionary<string, System.Text.Json.Nodes.JsonNode?>> TaskSettings { get; set; } = new();
    // Task ids in the player's order; tasks missing here follow in the data's order.
    public List<string> Order { get; set; } = new();

    public TaskProfile Copy(string name) => new()
    {
        Name = name,
        SelectedTasks = new List<string>(SelectedTasks),
        TaskSettings = TaskSettings.ToDictionary(kv => kv.Key, kv => kv.Value.ToDictionary(e => e.Key, e => e.Value?.DeepClone())),
        Order = new List<string>(Order),
    };
}

// Settings live in the user's profile (%APPDATA%\MabaAssistantBlueArchive\settings.json),
// never in the repository.
public sealed class SettingsStore
{
    private static readonly JsonSerializerOptions Options = new() { WriteIndented = true };

    public static string FilePath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "MabaAssistantBlueArchive", "settings.json");

    public static AppSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return JsonSerializer.Deserialize<AppSettings>(File.ReadAllText(FilePath)) ?? new AppSettings();
        }
        catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
        {
            // Unreadable settings fall back to defaults; the file is rewritten on the next save.
        }
        return new AppSettings();
    }

    public void Save(AppSettings settings)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
        var tmp = FilePath + ".tmp";
        File.WriteAllText(tmp, JsonSerializer.Serialize(settings, Options));
        File.Move(tmp, FilePath, overwrite: true);
    }

    // The data root holds pages/, tasks/, config/: data/ beside the executable in a release, the
    // repository root during development (searched upward from the executable).
    // The program's own data always wins. The settings file is shared by every copy of the program
    // on this PC, so a saved path may belong to another copy (a release run before a development
    // build, or an older release): it is used only when this copy has no data of its own.
    public static string ResolveDataRoot(string configured)
    {
        var release = Path.Combine(AppContext.BaseDirectory, "data");
        if (IsDataRoot(release)) return release;
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        for (var i = 0; dir is not null && i < 8; ++i, dir = dir.Parent)
        {
            if (IsDataRoot(dir.FullName)) return dir.FullName;
        }
        if (!string.IsNullOrWhiteSpace(configured) && IsDataRoot(configured)) return configured;
        return AppContext.BaseDirectory;
    }

    private static bool IsDataRoot(string path) =>
        Directory.Exists(Path.Combine(path, "tasks")) && Directory.Exists(Path.Combine(path, "pages")) &&
        File.Exists(Path.Combine(path, "config", "languages.json"));
}
