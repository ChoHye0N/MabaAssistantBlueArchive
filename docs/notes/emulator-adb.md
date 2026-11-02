# Emulator control over ADB: behavior notes

Notes in our own words, written before implementing `core/src/adb.cpp` and `core/src/adb_controller.cpp`. Several behaviors below follow the emulator controller of an AGPL-3.0 project; the porting log in `docs/SOURCES.md` names it. Items marked **verify** were not tested against a real emulator.

## Connecting
- Network serials (`host:port`, e.g. MuMu 12 `127.0.0.1:16384`) need `adb connect <serial>` first; local ones (`emulator-5554`) do not.
- `adb devices` lists `serial<TAB>state`. Only `device` is usable; `offline` and `unauthorized` must be reported as such.
- `adb -s <serial> shell wm size` prints `Physical size: WxH` and, when the resolution was changed, `Override size: WxH`; the override is the real one. Some emulators report portrait numbers for a landscape screen, so the larger side is the width.
- The game needs a 16:9 screen; reject anything else with a message that names supported resolutions.

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
