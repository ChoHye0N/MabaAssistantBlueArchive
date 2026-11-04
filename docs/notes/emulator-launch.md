# Starting the emulator and the game: behavior notes

Notes in our own words, written before implementing `core/src/emulator.cpp`. The start/stop sequence follows an MIT-licensed Blue Archive helper; the porting log in `docs/SOURCES.md` names it. Items marked **verify** were not tested against a real emulator.

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
- Closing: post `WM_CLOSE` to the window, wait up to 20 s for it to go away, then end the process tree of the window's process. **Verify** whether the client asks for a quit confirmation on `WM_CLOSE`; if it does, the kill is the normal path.
- Not done: closing Steam itself, and Steam dialogs shown before the game (updates, cloud sync conflicts); a run then fails with the window timeout.

## Failure handling
- Every adb call has a timeout. Every wait has a cap (boot timeout, game timeout) and checks the stop flag at each poll.
- If the emulator process ended right after its start, the emulator was usually already running or the start failed; the device poll decides.
