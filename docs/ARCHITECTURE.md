# Architecture

## Overview
```
GUI (C#, WPF, HandyControls)  --P/Invoke, C ABI (functions + JSON)--  Core (C++)
                                                                        |- Controller   capture + input, per platform
                                                                        |- Vision       template matching, calls IOcr
                                                                        |- Navigator    page detection, page graph, popup handler
                                                                        |- Task Engine  tasks/*.json steps, dependencies
                                                                        '- OCR Bridge   Python sidecar (RapidOCR, ONNX Runtime)
```
`Assistant` (`assistant.h`) is the composition root: it loads the data (`pages/`, `popups/`, `tasks/`, `config/languages.json`), owns the controller, matcher and OCR, and runs tasks. The C ABI and `maba_cli run` both use it.

Core and the GUI talk only through the C ABI, so either side can change alone. Python is for OCR and tools only: game flow never moves into Python. The controller interface stays small (`Screenshot`, `CaptureRegion`, `Click`, `Swipe`, `Scroll`, `Back`, `Error`), one implementation per platform.

## Controllers
| Target | Capture | Input | Notes |
|---|---|---|---|
| Steam PC client (first) | Window capture: BitBlt and PrintWindow, both implemented and selectable | Real mouse movement (SendInput-style), not window messages | DPI awareness and UI scale must be handled inside the controller. |
| Emulators over ADB (second) | `screencap` raw or PNG, the faster one picked on connect | `adb shell input`, or the MaaTouch helper (minitouch protocol) | `adb_controller.h`; notes in `docs/notes/controllers.md`. |

Steam PC controller (`core/src/platform/windows/steam_controller.h`):
- Finds the window by class and title (defaults `UnityWndClass` / `Blue Archive`, to verify). Rejects a minimized window or a client area that is not 16:9.
- Capture: `BitBlt` copies the client area from the screen (fast; the window must be visible and unobscured). `PrintWindow` asks the window to render itself (works when obscured, not when minimized; some DirectX windows return black, which is reported as an error). Selectable per call (`maba_cli --capture`).
- Input: real cursor movement with `SendInput`, a 100 ms pause between moving and pressing (the game reads the cursor once per frame) in virtual-desktop coordinates (multiple monitors). Before input the game window is brought to the front (if Windows refuses the focus change, a lone Alt press makes the request count as user input) and must be the foreground window, and no other window may cover the target point; otherwise the action fails instead of clicking elsewhere. The cursor is restored afterwards. If the game runs as administrator, Windows blocks input from a normal process (UIPI). `Back` sends Esc (to verify).
- DPI: every call runs with per-monitor DPI awareness on its thread, so window rects, client size and cursor coordinates are physical pixels at any display scale.
- `maba_cli info` checks both capture methods; `maba_cli grab` saves a normalized frame (or a native region) under `captures/`.

ADB controller (`core/include/maba/adb_controller.h`, needs OpenCV):
- Connect: `adb connect` for `host:port` serials, `adb devices` state check, `wm size` (override wins, larger side is the width), 16:9 check, screencap probe (raw and PNG, keep the faster), optional MaaTouch start.
- Capture repairs CRLF-mangled output of old adb builds and checks the raw header (12 or 16 bytes) and the alpha of the last pixel. One reconnect attempt per failed capture.
- Input: `input tap/swipe/keyevent`, or MaaTouch (`d/m/u/c/w` and `k <code> d|u`) scaled to the helper's touch range. `Back` sends keycode 111 (ESCAPE) by default; 4 (BACK) is the alternative (to verify).
- All adb calls go through `IProcessRunner` with timeouts; a timed-out call is killed with its process tree. The pieces without OpenCV (`adb.h`) are unit-tested on every platform.

Emulator start and stop (`core/include/maba/emulator.h`, portable, notes in `docs/notes/controllers.md`):
- `EmulatorSession::EnsureDevice`: when the device is not listed and an emulator path is set, starts the emulator detached (`IDetachedProcesses`, survives the GUI), polls `adb connect`/`adb devices` (an offline network device is disconnected and reconnected), then waits for `sys.boot_completed`. Bounded by the boot timeout (180 s) and the stop flag. A MuMu launcher path (`nx_main/MuMuNxMain.exe`) is rewritten to the instance executable.
- `EnsureGame`: reads the app in front from `dumpsys window`; if it is not the game, `am start -n` (fallback `monkey` with the launcher intent), repeated every 15 s within the game timeout (90 s). The title screen is then handled by `Navigator::EnterGame`.
- After the run (options): `am force-stop` the game; kill the process tree of the emulator, only if this session started it. A run stopped by the user leaves both running.
- Used by `MabaStart` (`connect` + `launch` + `finish`, all on the worker thread) and `maba_cli run --emulator ... --start-game`.

Steam client start and close (`core/include/maba/steam_launch.h`, portable logic; window access through `IGameWindowHost`, Win32 in `core/src/platform/windows/game_window_win32.cpp`, a stub elsewhere):
- `SteamSession::EnsureGame`: if the game window is missing, opens `steam://rungameid/3557620` with the shell and polls for the window (180 s, stop flag), then waits 5 s before input. A minimized window is restored, also when the game was already running (a minimized window cannot be captured).
- `CloseGame` (option): `WM_CLOSE`, then ends the process tree after 20 s. Not after a run stopped by the user.
- Used by `MabaStart` with `connect` type steam and `maba_cli run --controller steam --start-game --close-game`.

Contract (`controller.h`): frames are returned scaled to **1280x720** and all coordinates are normalized. `Scroll` turns the mouse wheel where the platform has one (the Steam client: it zooms the cafe out); ADB controllers return false. `CaptureRegion(Rect)` returns a crop taken at native resolution, for OCR where the downscaled frame may lose text detail. `coords.h` holds the mapping and is unit-tested. Reason: UI scale mismatches (e.g. Steam UI at ~0.8x of template size) otherwise break every layer above.

The Steam window can be resized freely. At the start of a run a window that is not 16:9 or smaller than 1280x720 is changed to 1280x720 (`fit_window`).

## Navigator
- Page graph (`navigator.h`): nodes are pages recognized by anchors, edges are their transitions. `FindPath` is a breadth-first search; ties follow the order of transitions in the file. `ValidatePageGraph` reports transitions to unknown pages.
- Current page: every page is checked; the detected page with the highest anchor score wins, ties go to the page with more anchors.
- **Global popup handler** (`popup.h`, data in `popups/*.json`): before every look at the screen, known popups are dismissed (click a template, a fixed point, or Back), at most `max_dismissals` per look. Tasks never handle popups themselves. Popups may also require or exclude colours (`colors`, `no_colors`): the battle HUD is kept on AUTO and 3x speed this way, by clicking a white (off) AUTO or speed button. Story scenes are popups too: a scene (MENU at the top right) is answered with Back, which opens the skip confirmation, which is confirmed; battle result screens are confirmed the same way. So a task only waits for its page to come back after a story stage or a battle.
- `EnterGame()`: start-up, once per run before the first task. While no known page is visible (title, event countdown splash, attendance board), tap the home page's `enter` point, bounded by `max_taps`; login popups are dismissed by the popup handler on every look. It stops at the first known page.
- `GoTo(target)`: look (popups settled) -> unknown screen: press Back -> known page: take the first transition of the shortest path, wait for a different page. Bounded by `max_steps` (clicks + backs) and `max_back_presses`; a transition target that is not visible spends a step (animations). `ReturnHome()` is `GoTo("main_menu")`. After arriving by a click it pauses `after_arrive` (700 ms): the game ignores input while a page is still sliding in.
- Pages with `wait_ms` end by themselves (a battle): GoTo waits and never presses Back, because Back opens the pause menu.
- Screen state, not time: `screen_change.h` measures the share of strongly changed pixels (at rest 0 to 11 %, page changes 60 % and more). `CheckedAction` in `task_runner.cpp` repeats a click or back press that changed nothing and fails the step after that, and `WaitSettled` waits for the screen to rest, treating a black loading screen as not resting. The assistant turns both on; unit tests replay fixed scenes and opt in.
- Every wait/retry/loop has a cap and timeout; on exceeding: the last frame is passed to `on_failure` (saved under `captures/`) and the task fails. A stop request (`cancel`) ends every wait at its next check.

## Task engine
(Page, popup, task and string data format: see `PAGE_FORMAT.md`.)

- Tasks are data in `tasks/*.json`: an `order` number (plain sequencing, never blocks), `after` dependencies (a failed one blocks) plus a list of declarative `steps` (navigation, clicks with `pick`/`offset`/`expect`, `click_color`, `swipe`, `read_number`, and flow control: `if`, `loop`, `each`, `break`, `end`, `fail`; format in `PAGE_FORMAT.md`). Conditions test settings, templates, colour blobs and numbers read by OCR. `ResolveOrder` (`task_order.h`) produces the run order and rejects duplicates, unknown dependencies and cycles.
- Per-account records: `save` steps write JSON records under a data directory outside the repository (GUI: the profile folder next to `settings.json`; CLI: `local-data/`, gitignored). `account_info` keeps the nickname, level, credits, pyroxene and AP per UID with a history; the GUI shows one box per record on the My info tab.
- Task settings (`settings` in the task file: bool, int, choice, list) have defaults; a run overrides them (`ResolveSettings`, from the GUI, `MabaStart` or `maba_cli run --settings`). Steps branch and loop on them.
- `TaskRunner` (`task_runner.h`): enters the game from start-up screens once (`EnterGame`), returns home before each task and after a failure (recovery); runs the steps with their caps; a step marked `optional` only logs its failure. A task without steps is skipped as "not implemented" and does not block dependents. A selected dependency that failed blocks its dependents (mail after AP spending).
- Required ordering rule: mail runs after AP spending, because AP above the 999 cap waits in the mailbox.
- Daily run order (owner): cafe, attendance book, schedule, circle, daily free package, crafting, shop, bounty, AP purchase, commissions, scrimmage, event, campaign missions, tactical challenge, mail, daily missions, battle pass, account record. `RepoTaskFilesLoad` in `core/tests/task_tests.cpp` checks it.
- Scheduling is keyed to the server reset time (a setting), not local midnight: `schedule.h` computes the daily slot (reset + delay) in UTC; the GUI asks Core through `MabaScheduleNext`. A clock-time mode runs at a local time instead (the GUI passes the current UTC offset); a missed slot runs as soon as the GUI is open, and the GUI warns when the time is less than an hour before the reset.

## Vision and OCR
- Colour blobs (`color.h`, portable): pixels inside an HSV range, connected and filtered by size. For markers whose shape animates and for states template matching cannot see (it ignores brightness, so a dimmed card still matches).
- Template matching (OpenCV) is the default. Icon-identifiable UI never needs OCR.
- OCR only for AP value, ticket counts, shop items/prices. Prefer reading digits only (tight ROI on the number): digits are language-independent. `ocr_text.h` turns text into numbers conservatively (`1,234`, `120/240`; anything ambiguous is rejected). Core depends only on `IOcr`.
- OCR engine: **RapidOCR on ONNX Runtime** with PaddleOCR models (PP-OCRv5 mobile for Korean, English and Thai; PP-OCRv4 for Traditional Chinese and Japanese), run by the Python sidecar `ocr/server.py`. Same models as PaddleOCR without the PaddlePaddle framework, and the ONNX files can later run natively in C++ through ONNX Runtime with no Python.

### OCR sidecar protocol
One JSON object per line over the child's stdin/stdout (`ocr_sidecar.h`, `ocr/server.py`). stdout belongs to the protocol; logs go to stderr.
```
sidecar -> {"ready": true, "engine": "rapidocr", "version": "3.10.0", "protocol": 1}
core    -> {"id": 1, "op": "recognize", "lang": "korean", "width": W, "height": H,
            "format": "bgr24", "data": "<base64>", "detect": true}
sidecar -> {"id": 1, "ok": true, "results": [{"text": "120/240", "box": [x, y, w, h], "score": 0.99}]}
           {"id": 1, "ok": false, "error": "..."}              (the loop keeps serving)
core    -> {"id": 2, "op": "ping"} | {"id": 3, "op": "shutdown"}
```
A startup failure is reported as `{"error": "..."}` instead of the ready line. Core skips stale responses by id, restarts a crashed sidecar at most `max_restarts` times, and times out every request.

## C ABI
`core/include/maba/maba_api.h`, versioned by `MabaApiVersion()` (currently 5: clock-time mode in `MabaScheduleNext` and Steam launch in `MabaStart`; 4 added emulator launch). Functions take and return UTF-8 JSON; returned strings stay valid until the next call on the same handle. A change of the contract raises `MabaApiVersion()` and the GUI's `ExpectedApiVersion` together.

| Function | Purpose |
|---|---|
| `MabaCreate(config, callback, user)` | `{"root", "language", "data_dir", "ocr": {"enabled", "command"}}`; loads the data; errors via `MabaLastError` |
| `MabaGetTasks` | `[{"id", "name", "order", "after", "status", "implemented", "needs_ocr", "settings": [{"key", "type", "default", "min", "max", "choices", "description", "ui", "labels"}]}]` |
| `MabaConnect(json)` | `{"type": "steam", "capture"}` or `{"type": "adb", "adb_path", "serial", "screencap", "touch", "maatouch_path", "back_keycode"}` |
| `MabaStart({"tasks": [...], "settings": {"<task id>": {"<key>": value}}, "connect", "launch", "finish"})`, `MabaStop`, `MabaIsRunning` | run on a worker thread; stop is cooperative and bounded; settings are validated before the run starts. Optional `connect` (MabaConnect arguments) connects on the worker; with type `adb`, `launch` (`emulator_path`, `emulator_args`, `start_game`, `game_activity`, `boot_timeout_s`, `game_timeout_s`) starts the emulator and the game first and `finish` (`close_game`, `close_emulator`) closes them after a run that was not stopped |
| `MabaListAdbDevices(adb_path)` | `[{"serial", "state"}]` |
| `MabaScheduleNext(json)` | `{"due", "next", "slot"}` for the daily schedule; `"mode": "clock"` with `hour`, `minute`, `utc_offset_minutes` runs at a local time and adds `minutes_before_reset` |

Events (callback, worker thread): `{"type": "log", "level", "message"}`, `{"type": "task", "id", "status", "message", "values"}`, `{"type": "run", "status": "started" | "finished", "ok", "cancelled", "error"}`, `{"type": "failure_frame", "path", "reason"}`.

## GUI
`gui/Maba.Gui` (WPF, .NET 10, HandyControls theme and controls, CommunityToolkit.Mvvm). It talks to Core only through the C ABI (`Native/NativeMethods.cs`, `Native/CoreClient.cs`). UI text lives in `Resources/Strings.<lang>.xaml`: Korean (baseline) and English. The display language is a setting (default: the Windows language) applied at start-up, before the view models read their text; another language is merged on top of the Korean table, so a missing key shows the Korean text. `tools/check_ui_strings.py` checks that every table has the baseline's keys and placeholders. The display language is separate from the game client language (`Language`: templates, OCR, `strings/`), and English UI text follows the global client's English terms (Café, Schedule, Club, Bounty, Tactical Challenge, Tasks, Mailbox, Sweep). Settings are stored in the user profile (`%APPDATA%\MabaAssistantBlueArchive\settings.json`), never in the repository. Each task shows its settings (from `MabaGetTasks`) under an expander; values are stored with the other settings and sent with `MabaStart`. The scheduler checks every 30 s whether the daily slot is due and starts the selected tasks; a scheduled day counts as done even after failures (no retry loop).

Updates: `Services/ResourceUpdater.cs` installs newer resources (task data and templates) from `resources-<version>` releases without a restart. `Services/ProgramUpdater.cs` downloads the changed parts of a new program release and hands over to `maba_updater.exe` (`core/apps/maba_updater.cpp`, logic in `core/src/update_apply.cpp`), which replaces the files after the GUI has exited and starts it again; `Services/UpdateChecker.cs` is the plain notice for installations without the updater. `Services/UpdateFiles.cs` holds the shared download, checksum and unpacking code. Rules: `docs/notes/updates.md`.

## Languages
- Korean (`ko`) is the baseline for behavior and design; other languages are added later. UI text that logic compares lives in `strings/<lang>.json` by key, never as literals in code.
- `config/languages.json` lists languages and their OCR model codes; confirmed for the Steam global client: Korean, English, Traditional Chinese, Thai. Japanese is unverified.
- Template lookup: `assets/<lang>/` then `assets/common/`. Language-neutral anchors (icons, layout) go to common.
- `tools/check_templates.py` prints the task x language matrix (templates each task needs, including the pages on its route and all popups); CI fails if a task marked `ready` misses a template in a confirmed language.

## Platforms and testing
- Portable code (`core/src/*.cpp` except platform) builds and tests on Linux/Windows. Child processes have a Win32 and a POSIX implementation (`process.h`).
- Windows-only code lives in `core/src/platform/windows/` and `gui/`.
- Controllers, matchers, clocks, adb and the OCR peer are faked in tests (`core/tests/fakes.h`, `nav_rig.h`); real screenshots are checked by `maba_fixture_tests` when present.

## Out of scope for now
Total Assault and Limit-Release Showdown (they need AI or real gameplay). Story is only skipped, never read or chosen. Model selection and training are decided later.
