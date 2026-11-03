// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Maba.Gui.Native;

public sealed record CoreTask(string Id, string Name, IReadOnlyList<string> After, string Status,
                              bool Implemented, bool NeedsOcr, IReadOnlyList<CoreSetting> Settings);

// A task setting declared in tasks/<id>.json (type: bool, int, choice, list).
public sealed record CoreSetting(string Key, string Type, JsonNode? Default, int? Min, int? Max,
                                 IReadOnlyList<string> Choices, string Description, string Ui,
                                 IReadOnlyDictionary<string, string> Labels);

public sealed record AdbDevice(string Serial, string State);

// MinutesBeforeReset: clock mode only, from the run slot to the next server reset.
public sealed record ScheduleInfo(bool Due, DateTimeOffset Next, int? MinutesBeforeReset);

// Managed wrapper around one native instance. Events arrive on a worker thread as JSON;
// `EventReceived` is raised on that thread, so subscribers marshal to the UI themselves.
public sealed class CoreClient : IDisposable
{
    private IntPtr _handle;
    private readonly NativeMethods.EventCallback _callback;  // kept alive while native code holds it
    private readonly IntPtr _callbackPtr;

    public event Action<JsonObject>? EventReceived;

    public string Version { get; }

    public CoreClient(string dataRoot, string language, bool ocrEnabled)
    {
        var api = NativeMethods.MabaApiVersion();
        if (api != NativeMethods.ExpectedApiVersion)
            throw new InvalidOperationException(
                $"MabaCore.dll C ABI version {api} does not match the GUI ({NativeMethods.ExpectedApiVersion}).");
        Version = NativeMethods.Utf8(NativeMethods.MabaGetVersion());
        _callback = OnNativeEvent;
        _callbackPtr = Marshal.GetFunctionPointerForDelegate(_callback);
        var config = new JsonObject
        {
            ["root"] = dataRoot,
            ["language"] = language,
            // Per-account records (UID, owned students, resources) stay in the user profile.
            ["data_dir"] = System.IO.Path.GetDirectoryName(Services.SettingsStore.FilePath),
            ["ocr"] = new JsonObject { ["enabled"] = ocrEnabled },
        };
        _handle = NativeMethods.MabaCreate(config.ToJsonString(), _callbackPtr, IntPtr.Zero);
        if (_handle == IntPtr.Zero) throw new OutOfMemoryException("MabaCreate failed");
    }

    public string LastError => NativeMethods.Utf8(NativeMethods.MabaLastError(Handle));

    public bool IsRunning => _handle != IntPtr.Zero && NativeMethods.MabaIsRunning(_handle) != 0;

    public IReadOnlyList<CoreTask> GetTasks()
    {
        var json = NativeMethods.Utf8(NativeMethods.MabaGetTasks(Handle));
        var list = new List<CoreTask>();
        foreach (var node in JsonNode.Parse(json)?.AsArray() ?? new JsonArray())
        {
            if (node is not JsonObject o) continue;
            list.Add(new CoreTask(
                o["id"]?.GetValue<string>() ?? "",
                o["name"]?.GetValue<string>() ?? "",
                o["after"]?.AsArray().Select(a => a?.GetValue<string>() ?? "").ToList() ?? new List<string>(),
                o["status"]?.GetValue<string>() ?? "",
                o["implemented"]?.GetValue<bool>() ?? false,
                o["needs_ocr"]?.GetValue<bool>() ?? false,
                (o["settings"]?.AsArray() ?? new JsonArray()).OfType<JsonObject>().Select(s => new CoreSetting(
                    s["key"]?.GetValue<string>() ?? "",
                    s["type"]?.GetValue<string>() ?? "",
                    s["default"]?.DeepClone(),
                    s["min"]?.GetValue<int>(),
                    s["max"]?.GetValue<int>(),
                    s["choices"]?.AsArray().Select(c => c?.GetValue<string>() ?? "").ToList() ?? new List<string>(),
                    s["description"]?.GetValue<string>() ?? "",
                    s["ui"]?.GetValue<string>() ?? "",
                    (s["labels"] as JsonObject)?.ToDictionary(kv => kv.Key, kv => kv.Value?.GetValue<string>() ?? kv.Key)
                        ?? new Dictionary<string, string>())).ToList()));
        }
        return list;
    }

    public bool Connect(JsonObject args) => NativeMethods.MabaConnect(Handle, args.ToJsonString()) != 0;

    // `settings`: {"<task id>": {"<key>": value}}; Core rejects unknown keys and wrong types.
    // `prepare` (optional) adds "connect", "launch" and "finish": Core connects on its worker,
    // after starting the emulator and the game.
    public bool Start(IEnumerable<string> taskIds, JsonObject settings, JsonObject? prepare = null)
    {
        var args = new JsonObject
        {
            ["tasks"] = new JsonArray(taskIds.Select(t => (JsonNode?)t).ToArray()),
            ["settings"] = settings,
        };
        if (prepare is not null)
        {
            foreach (var (key, value) in prepare) args[key] = value?.DeepClone();
        }
        return NativeMethods.MabaStart(Handle, args.ToJsonString()) != 0;
    }

    public bool Stop() => NativeMethods.MabaStop(Handle) != 0;

    public IReadOnlyList<AdbDevice> ListAdbDevices(string adbPath)
    {
        var json = NativeMethods.Utf8(NativeMethods.MabaListAdbDevices(Handle, adbPath));
        return (JsonNode.Parse(json)?.AsArray() ?? new JsonArray())
            .OfType<JsonObject>()
            .Select(o => new AdbDevice(o["serial"]?.GetValue<string>() ?? "", o["state"]?.GetValue<string>() ?? ""))
            .ToList();
    }

    // Daily schedule computed by Core (core/include/maba/schedule.h): `mode` "reset" runs
    // `delayMinutes` after the server reset, "clock" runs at `hour`:`minute` local time.
    public static ScheduleInfo? NextRun(DateTimeOffset now, DateTimeOffset? lastRun, int resetHourUtc,
                                        int resetMinuteUtc, int delayMinutes,
                                        string mode = "reset", int hour = 0, int minute = 0)
    {
        var args = new JsonObject
        {
            ["now"] = now.ToUnixTimeSeconds(),
            ["last_run"] = lastRun?.ToUnixTimeSeconds() ?? 0,
            ["reset_hour_utc"] = resetHourUtc,
            ["reset_minute_utc"] = resetMinuteUtc,
            ["delay_minutes"] = delayMinutes,
            ["mode"] = mode,
        };
        if (mode == "clock")
        {
            args["hour"] = hour;
            args["minute"] = minute;
            // The offset in effect now, so a daylight-saving change follows the PC clock.
            args["utc_offset_minutes"] = (int)TimeZoneInfo.Local.GetUtcOffset(now).TotalMinutes;
        }
        var res = JsonNode.Parse(NativeMethods.Utf8(NativeMethods.MabaScheduleNext(args.ToJsonString())))?.AsObject();
        if (res is null || res["ok"]?.GetValue<bool>() != true) return null;
        return new ScheduleInfo(res["due"]!.GetValue<bool>(),
                                DateTimeOffset.FromUnixTimeSeconds(res["next"]!.GetValue<long>()),
                                res["minutes_before_reset"]?.GetValue<int>());
    }

    private IntPtr Handle => _handle != IntPtr.Zero ? _handle : throw new ObjectDisposedException(nameof(CoreClient));

    private void OnNativeEvent(IntPtr eventJson, IntPtr userData)
    {
        try
        {
            if (JsonNode.Parse(NativeMethods.Utf8(eventJson)) is JsonObject obj) EventReceived?.Invoke(obj);
        }
        catch (JsonException)
        {
            // A malformed event must never cross back into native code as an exception.
        }
    }

    public void Dispose()
    {
        if (_handle == IntPtr.Zero) return;
        NativeMethods.MabaDestroy(_handle);  // stops and joins the worker before returning
        _handle = IntPtr.Zero;
        GC.KeepAlive(_callback);
    }
}
