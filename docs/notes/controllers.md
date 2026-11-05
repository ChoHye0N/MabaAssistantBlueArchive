# Controlling the game: behavior notes

Notes in our own words, written before implementing the controllers and the start-up code. The porting log in `docs/SOURCES.md` names what they follow. Items marked **verify** were not tested on a real emulator.

# ADB

## Connecting
- Network serials (`host:port`, e.g. MuMu 12 `127.0.0.1:16384`) need `adb connect <serial>` first; local ones (`emulator-5554`) do not.
- `adb devices` lists `serial<TAB>state`. Only `device` is usable; `offline` and `unauthorized` must be reported as such.
- `adb -s <serial> shell wm size` prints `Physical size: WxH` and, when the resolution was changed, `Override size: WxH`; the override is the real one. Some emulators report portrait numbers for a landscape screen, so the larger side is the width.
- The game window can be resized freely. A run needs 16:9 and at least 1280x720: at the start it changes any other window to 1280x720 (`fit_window`), and a size it cannot use is rejected with a message. Confirmed: the game accepts a size set from outside.

Default serials (all **verify**, they change with versions and instance numbers):

| Emulator | Serial |
|---|---|
| MuMu Player 12 | `127.0.0.1:16384` (+32 per extra instance) |
| LDPlayer 9 | `emulator-5554` (or `127.0.0.1:5555`) |
| BlueStacks 5 | `127.0.0.1:5555` (port shown in its ADB settings) |

## Screen capture
- `adb exec-out screencap` (raw): a little-endian header of 12 bytes (width, height, pixel format) or 16 bytes (plus color space, newer Android), followed by width x height RGBA pixels. The header size is whatever is left after the pixel data. The last pixel's alpha must be 255; anything else means the data is damaged.
- `adb exec-out screencap -p` (encode): a PNG. Slower on the device side, smaller to transfer.
- Old adb builds on Windows turn every `\n` into `\r\n` in shell output. If decoding fails, replace `\r\n` with `\n` and retry; remember that the connection needs it.
- On connect, try both methods once and keep the faster one (the cost differs a lot between emulators). The capture's own size is the source of truth for input scaling.
- Faster channels exist (netcat raw stream, gzip-compressed raw, emulator-specific shared-memory APIs for MuMu and LDPlayer). Not implemented yet.

## Input
- `adb shell input tap x y`, `input swipe x1 y1 x2 y2 ms`, `input keyevent <code>`: works everywhere, but each call starts a process on the device (slow, around 100 ms or more).
- Touch helpers that speak the minitouch text protocol are faster and support real swipes. MaaTouch (Apache-2.0) runs through `app_process`, so it works on recent Android where the old native minitouch binary does not:
  - push the file to `/data/local/tmp/maatouch`, then run `adb shell "export CLASSPATH=/data/local/tmp/maatouch; app_process /data/local/tmp com.shxyke.MaaTouch.App"` and keep stdin/stdout open.
  - On start it prints `v <version>`, `^ <contacts> <max-x> <max-y> <max-pressure>`, `$ <pid>`. Some emulators swap max-x and max-y; for a landscape game take the larger as x.
  - Commands: `d <contact> <x> <y> <pressure>`, `m ...`, `u <contact>`, `c` (commit), `w <ms>` (wait), `r` (reset); MaaTouch adds `k <keycode> d|u` for keys. Coordinates are in the helper's range (`max-x`, `max-y`), scaled from screen pixels.
- Back: keycode 111 (ESCAPE) or 4 (BACK). Which one the game treats as "back" on every page is **verify**; it is a setting.

## Failure handling
- Every adb call has a timeout; a timed-out call is killed with its process tree.
- One reconnect attempt per failed capture (the emulator may have restarted), then the error goes up to the Navigator, which fails the task with a saved frame.

# Starting and closing

## Sequence of a run
1. **Device present?** Network serials (`host:port`) get `adb connect` first. The device is ready when `adb devices` lists it as `device`. An `offline` network device is disconnected (`adb disconnect <serial>`) and connected again on the next poll.
2. **Start the emulator** only when the device is missing and an emulator path is configured. The emulator is started detached (it must outlive a failed run and the GUI), with its own folder as the working directory. Its process id is kept so that only an emulator this run started is closed later.
3. **Wait for the device:** poll `adb connect` / `adb devices` every few seconds until the boot timeout (default 180 s). Then wait for `getprop sys.boot_completed` to print `1` (the device is listed long before Android has finished booting).
4. **Start the game** if it is not in front: `am start -n <package>/<activity>`; if that prints an error, `monkey -p <package> -c android.intent.category.LAUNCHER 1` (needs no activity name). Repeat the start command every 15 s; give up after the game timeout (default 90 s).
   - Global client: `com.nexon.bluearchive/.MxUnityPlayerActivity` (**verify** on each emulator).
5. **Which app is in front:** `dumpsys window`, the line `mCurrentFocus=Window{<id> u0 <package>/<activity>}`. Use the last such line that is not `null`; some emulators (MuMu with "keep alive") report `null`, then fall back to `mFocusedApp=ActivityRecord{<id> u0 <package>/<activity> t<n>}`. A focused window without an activity (`Window{<id> u0 <package>}`) still names the package.
6. The task runner then enters the game from the title screen (`Navigator::EnterGame`).
7. **After the run** (options): close the game with `am force-stop <package>`; close the emulator by killing the process tree that was started in step 2 (`taskkill /T /F /PID <pid>`). A manually stopped run leaves both running.

## Emulators
- **MuMu (versions with `nx_main`)**: the desktop shortcut points to `nx_main/MuMuNxMain.exe`, which opens the multi-instance manager instead of an instance. The instance itself is `nx_device/<android version>/shell/MuMuNxDevice.exe` with the same arguments (`-v <index>` picks the instance). The Android version comes from, in order: an `--engine_series=<version>` argument (removed from the arguments), `nx_main/MuMuManager.exe info -v <index>` (JSON with `android_version`), or the newest folder under `nx_device/`. `--from-shortcut` is dropped. (**verify**)
- **Killing the started process may leave a background VM process** behind on some emulators (MuMu). Not handled yet (**verify**); the emulator's own manager command would be the clean way.
- LDPlayer 9 (`dnplayer.exe`, `index=<n>` argument) and BlueStacks 5 (`HD-Player.exe --instance <name>`): started as configured, no path rewriting. (**verify**)

## Steam client
- `steam://rungameid/3557620` starts the global client through Steam; the window title is `Blue Archive`, the process `BlueArchive.exe`. Skip the start when the window already exists.
- Our own sequence (`core/include/maba/steam_launch.h`): look for a visible `UnityWndClass` / `Blue Archive` window; if there is none, open the URL with the shell (Steam starts first when it is not running, so the wait is long: 180 s), poll for the window, then wait a few seconds more before input (the window exists before the game draws; **verify** 5 s is enough). The title screen is handled by `EnterGame` as for the emulator.
- Confirmed 2026-11-04: the app id starts the game (window after about 20 s). The game can open minimized when another window is in front; a minimized window cannot be captured, so the session restores it.
- Closing: post `WM_CLOSE` to the window, wait up to 20 s for it to go away, then end the process tree of the window's process. `WM_CLOSE` only opens the quit dialog, so the kill is the normal path.
- Not done: closing Steam itself, and Steam dialogs shown before the game (updates, cloud sync conflicts); a run then fails with the window timeout.

## Failure handling
- Every adb call has a timeout. Every wait has a cap (boot timeout, game timeout) and checks the stop flag at each poll.
- If the emulator process ended right after its start, the emulator was usually already running or the start failed; the device poll decides.
