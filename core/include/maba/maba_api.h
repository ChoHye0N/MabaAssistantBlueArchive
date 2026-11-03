// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// C ABI exposed to the C# GUI (P/Invoke, cdecl). Keep it small: functions + UTF-8 JSON strings.
// Returned strings are owned by the library: per handle they stay valid until the next call on
// the same handle; handle-less ones until the next call on the same thread. Never free them.
// Format of every JSON argument and event: docs/ARCHITECTURE.md, "C ABI".
#pragma once

#if defined(_WIN32)
#if defined(MABA_BUILDING_DLL)
#define MABA_API __declspec(dllexport)
#else
#define MABA_API __declspec(dllimport)
#endif
#define MABA_CALL __cdecl
#else
#define MABA_API __attribute__((visibility("default")))
#define MABA_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MabaInstance* MabaHandle;

// Events as JSON: {"type": "log" | "task" | "run" | "failure_frame", ...}. Called from the
// worker thread; the GUI must marshal to its UI thread (and must not call MabaStart or
// MabaDestroy from inside the callback). `event_json` is valid during the call.
typedef void(MABA_CALL* MabaEventCallback)(const char* event_json, void* user_data);

// Library version string, e.g. "0.1.0". The pointer is static and must not be freed.
MABA_API const char* MABA_CALL MabaGetVersion(void);

// Bumped on every breaking change to this C ABI.
MABA_API int MABA_CALL MabaApiVersion(void);

// Creates an instance and loads the data under config.root. Never returns null except when out
// of memory; if loading failed, MabaLastError says why and Start/Connect fail.
// config: {"root": "...", "language": "ko", "ocr": {"enabled": true, "command": [...]}}
MABA_API MabaHandle MABA_CALL MabaCreate(const char* config_json, MabaEventCallback callback,
                                         void* user_data);

// Stops a running job (waits for it) and frees the instance.
MABA_API void MABA_CALL MabaDestroy(MabaHandle handle);

// Reason for the last failure on this handle ("" if none).
MABA_API const char* MABA_CALL MabaLastError(MabaHandle handle);

// Task list as a JSON array: [{"id", "name", "after", "status", "implemented", "needs_ocr"}].
MABA_API const char* MABA_CALL MabaGetTasks(MabaHandle handle);

// Connects a controller. 1 on success, 0 on failure (see MabaLastError). Not while running.
// {"type": "steam", "capture": "bitblt" | "printwindow"}
// {"type": "adb", "adb_path": "adb", "serial": "127.0.0.1:16384", "screencap": "auto",
//  "touch": "adb" | "maatouch", "maatouch_path": "...", "back_keycode": 111}
MABA_API int MABA_CALL MabaConnect(MabaHandle handle, const char* json);

// Starts the selected tasks on a worker thread: {"tasks": ["cafe", ...]}. 1 if started.
// Optional: "settings" (task settings), "connect" (MabaConnect arguments: the worker connects
// before the run), and "launch"/"finish" (need "connect"):
//   emulator (type adb):
//   "launch": {"emulator_path": "...", "emulator_args": "..." | [...], "start_game": true,
//              "game_activity": "...", "boot_timeout_s": 180, "game_timeout_s": 90}
//     starts the emulator if its device is missing (and a path is set), waits for the boot and
//     brings the game to the front before connecting;
//   "finish": {"close_game": false, "close_emulator": false} after the run (not after a stop);
//     only an emulator started by this run is closed.
//   Steam client (type steam):
//   "launch": {"start_game": true, "steam_url": "steam://rungameid/3557620", "game_timeout_s": 180}
//     opens the URL when the game window is missing and waits for the window;
//   "finish": {"close_game": false} closes the game window (ends the process if it stays open).
MABA_API int MABA_CALL MabaStart(MabaHandle handle, const char* json);

// Requests a stop; the job ends at the next check (all waits are bounded). 1 if it was running.
MABA_API int MABA_CALL MabaStop(MabaHandle handle);

MABA_API int MABA_CALL MabaIsRunning(MabaHandle handle);

// `adb devices` as JSON: [{"serial", "state"}]. adb_path may be null ("adb").
MABA_API const char* MABA_CALL MabaListAdbDevices(MabaHandle handle, const char* adb_path);

// Daily schedule, once per day:
// in  {"now": unix, "last_run": unix|0, "reset_hour_utc": 19, "reset_minute_utc": 0,
//      "delay_minutes": 5, "mode": "reset"|"clock"}
//     mode "reset" (default): run `delay_minutes` after the server reset.
//     mode "clock": run at a local time; add {"hour": 0-23, "minute": 0-59,
//     "utc_offset_minutes": local offset now}. The reset fields still name the server reset.
// out {"ok": true, "due": bool, "next": unix, "slot": unix} or {"ok": false, "error": "..."};
//     mode "clock" adds "minutes_before_reset" (slot to the next server reset).
MABA_API const char* MABA_CALL MabaScheduleNext(const char* json);

#ifdef __cplusplus
}
#endif
