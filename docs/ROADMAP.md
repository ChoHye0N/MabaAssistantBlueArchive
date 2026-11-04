# Roadmap

## M-1: scaffold (done in this repo)
- [x] AGPL-3.0-only license, README, docs
- [x] Portable core: `types.h`, `coords.h` (1280x720 normalization), `task_order.h` (dependency order), `IController`, `IOcr`, C ABI stub
- [x] Unit tests (`ctest`), presets for Linux dev and Windows MSVC
- [x] Privacy check (`tools/privacy_check.py`) and a pre-commit hook (`.githooks/`)
- [x] Task definitions for the six daily tasks, `languages.json`, asset folders

## M0: first end-to-end on one platform
- [x] Windows Steam controller behind `IController`: BitBlt and PrintWindow capture (selectable), real mouse input, normalize to 1280x720 (`core/src/platform/windows/`)
- [x] Template matching in Vision (OpenCV), ROI support (`vision.h`)
- [x] Template capture tool (`tools/template_capture.py`): screenshot -> crop -> mask -> save with metadata
- [x] Main-screen detection and "open mail" click as a smoke test (`maba_cli smoke`, unit-tested with a fake controller)
- [x] Run against the real Steam client at 100% display scale: templates captured, `maba_cli info`, `smoke --dry-run` and `smoke` pass with both capture methods (2026-11-02)
- [ ] Same at 125% and 150% display scale
- [x] Templates rebuilt from 1920x1080 frames (full screen), normalized by Core itself; pixels that change between frames made transparent on 6 templates
- [ ] Recapture at 1920x1080 the 24 templates still cut from the older window: battle HUD and result, story, login notices and attendance, neighbour cafe invitation, event-specific screens
- [x] Templates cut from 1920x1080 frames also fit frames of a 1493x840 window: 76 templates seen in both, best score median 0.993, lowest 0.958
- [ ] Check a 1280x720 window

## M1: navigation + simplest tasks
- [x] Navigator: page graph, global popup handler, return-to-main, bounded GoTo (`navigator.h`, `popup.h`), unit-tested with scene fakes
- [x] Task engine: declarative steps, dependency-aware runner with recovery and stop (`task.h`, `task_runner.h`)
- [x] Mail claim, mission claim: steps written (`status: wip`)
- [ ] Capture their templates (`python tools/check_templates.py` lists them) and the real popups; run `maba_cli run --tasks mail_claim_all,mission_claim_all`; then mark them `ready`

## M2
- [x] Run order from data (`order`), task settings declared in `tasks/*.json`
- [x] Step language: conditions, loops, each over list settings, colour blobs, swipes, click pick/offset/expect (`flow_tests.cpp`)
- [x] Task settings read by Core (`ResolveSettings`), passed by `MabaStart`/`--settings`, edited in the GUI
- [x] Cafe, schedule, circle, daily free package, shop, bounty, tactical challenge: steps written and run in the game on 2026-11-02 (`wip`; see the open items below)
- [x] Commissions (Base Defense, Item Retrieval): sweeps a set number of times, before AP spending (`wip`); run in the game on 2026-11-04 with the highest stage and 2 sweeps each; picking a stage by number is not verified
- [x] Run the tasks with tickets and rewards available, 2026-11-04: all of them with the reaction check on, bounty sweep confirmed by the owner
- [ ] A few more days without failures, then mark the tasks `ready`
- [ ] Show in the result when `optional` steps failed: a target that did nothing still ended as succeeded (found with the campaign sweep)

## M3: OCR-dependent tasks
- [x] `IOcr` Python sidecar: RapidOCR on ONNX Runtime with PaddleOCR models, line-JSON protocol, restart and timeouts (`ocr_sidecar.h`, `ocr/server.py`)
- [x] Number parsing for OCR text (`ocr_text.h`), `read_number` step
- [x] Measure accuracy on labeled game crops, Korean on saved 1280x720 screenshots (`tools/measure_ocr.py`, `docs/notes/ocr-accuracy.md`)
- [x] Measure on native 1920x1080 crops; a 16 px margin around number crops (`read_number`)
- [ ] Other languages
- [x] Spend AP: event (Story stages with skipped scenes, then the highest 3-star Quest), missions, scrimmage, in the order of a setting; second pass after mail
- [ ] Detect 2x/3x reward campaigns and put their target first
- [ ] Push campaign areas that are not cleared yet (grid maps: team movement per stage, from a written stage guide)
- [x] Schedule locations chosen by name (12 locations, check boxes in the GUI)
- [ ] Schedule: prefer rooms with configured students across the chosen locations; otherwise spend tickets in list order
- [x] Account record per UID: credits, pyroxene, AP with history (`account_info`)
- [ ] On hold: record the owned students. Reading the student list with OCR gave misreads and noise; it needs a dictionary of student names per language to correct them (see `docs/notes/resources.md`)
- [ ] Verify in a live battle: AUTO/3x popups, event quest clearing with the configured team

## M4
- [x] Emulator controller over ADB: raw/PNG screencap with probe, `input` or MaaTouch, CRLF repair, reconnect (`adb_controller.h`), unit-tested with a fake adb
- [ ] Verify against MuMu 12, LDPlayer 9 and BlueStacks 5 (serials, back key, MaaTouch); faster capture channels later
- [x] Start the emulator when its device is missing, wait for the boot, start the game; close the game/emulator after the run (`emulator.h`, GUI "실행 준비", `maba_cli run --emulator`), unit-tested with fakes
- [ ] Verify the launch on real emulators: MuMu launcher rewrite, game activity, killing the emulator leaves no background VM
- [x] Steam client: start through `steam://rungameid/<app id>` and wait for the window; close after the run (`steam_launch.h`, GUI "실행 준비" for Steam), unit-tested with fakes
- [x] Steam launch in the real client: app id, a window that opens minimized
- [ ] Verify quit confirmation on `WM_CLOSE`, Steam not running or signed out
- [ ] Unattended runs without the GUI open (Task Scheduler entry or start with Windows, tray)
- [x] C# GUI (WPF, HandyControls, CommunityToolkit.Mvvm) over the C ABI, scheduler keyed to the server reset (`gui/`, `schedule.h`)
- [x] GUI display language: Korean and English (`Strings.en.xaml`, setting with the Windows language as default), keys checked by `tools/check_ui_strings.py`
- [ ] Check the English UI in the running GUI (text length in the settings layout); English game client support (templates, `strings/en.json`) is separate
- [x] Schedule at a local clock time as an alternative to "reset + delay", with a warning close to the reset (`ClockSchedule`)
- [x] Language matrix + CI template check (`tools/check_templates.py`, `.github/workflows/ci.yml`)

## Open questions to verify in the real game
- Confirmed 2026-11-02 (Korean region, 100% scale): window class `UnityWndClass`, title `Blue Archive`, 16:9 client area; BitBlt and PrintWindow both work; `SendInput` mouse input works.
- The first click right after returning to the main screen is sometimes ignored; the reaction check repeats it.
- Does Esc act as "back" on sub pages? It does nothing on the login event splash (that screen is tapped through, see `enter` in `pages/main_menu.json`).
- Emulator: keycode 111 (ESCAPE) or 4 (BACK) for "back"?
- Login sequence seen on 2026-11-02: image notices on the title screen, title, event countdown splash, attendance reward, attendance board, main screen, then a web news board a few seconds later and an "expiring items" notice. Only the first login of the day shows most of them.
- The claim-all buttons are greyed out when there is nothing to claim; the grey state scores 0.87 to 0.90 against the active template, so the steps use threshold 0.95.
- Does cafe income include AP? Assumed yes (cafe runs before AP spending); verify on the income panel.
- Daily-mission rewards include AP (daily login: 100 AP), and claimed AP can exceed the regular cap (212 -> 312 of 240).
- Server reset: mission deadlines read 03:59:59 KST, so the reset is 04:00 KST (19:00 UTC).
- Circle attendance (10 AP) is delivered to the mailbox.
- The "expiring items" notice shares its title with other dialogs; it is not registered as a popup yet (a confirm click could hit a purchase dialog).
- Do commissions have a daily limit? Sweeps of the highest stage cost 40 AP each; no limit was seen on 2026-11-04.
- Is Japanese really supported by the Steam global client? (store sources disagree). The Japan server has no PC client and is out of scope.
