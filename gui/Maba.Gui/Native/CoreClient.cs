// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Maba.Gui.Native;

public sealed record CoreTask(string Id, string Name, IReadOnlyList<string> After, string Status,
                              bool Implemented, bool NeedsOcr);

public sealed record AdbDevice(string Serial, string State);

public sealed record ScheduleInfo(bool Due, DateTimeOffset Next);

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
                o["needs_ocr"]?.GetValue<bool>() ?? false));
        }
        return list;
    }

    public bool Connect(JsonObject args) => NativeMethods.MabaConnect(Handle, args.ToJsonString()) != 0;

    public bool Start(IEnumerable<string> taskIds)
    {
        var args = new JsonObject { ["tasks"] = new JsonArray(taskIds.Select(t => (JsonNode?)t).ToArray()) };
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

    // Server-reset based schedule, computed by Core (core/include/maba/schedule.h).
    public static ScheduleInfo? NextRun(DateTimeOffset now, DateTimeOffset? lastRun, int resetHourUtc,
                                        int resetMinuteUtc, int delayMinutes)
    {
        var args = new JsonObject
        {
            ["now"] = now.ToUnixTimeSeconds(),
            ["last_run"] = lastRun?.ToUnixTimeSeconds() ?? 0,
            ["reset_hour_utc"] = resetHourUtc,
            ["reset_minute_utc"] = resetMinuteUtc,
            ["delay_minutes"] = delayMinutes,
        };
        var res = JsonNode.Parse(NativeMethods.Utf8(NativeMethods.MabaScheduleNext(args.ToJsonString())))?.AsObject();
        if (res is null || res["ok"]?.GetValue<bool>() != true) return null;
        return new ScheduleInfo(res["due"]!.GetValue<bool>(),
                                DateTimeOffset.FromUnixTimeSeconds(res["next"]!.GetValue<long>()));
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
