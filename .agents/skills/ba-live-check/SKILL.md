---
name: ba-live-check
description: Procedure for looking at the real game and verifying a task in it for the Blue Archive automation project. Use when exploring a screen that is new or changed after a game update, when cutting or checking a template, when reproducing a bug the maintainer saw in a run, and before telling the maintainer that a task works in the game.
---

# Live checks in the real game

The game is the maintainer's real account. A wrong click can spend pyroxene, AP or tickets, or open a real-money purchase. These rules exist because each one was broken once.

## Before anything runs
- The main screen hides its buttons after a while without input; that looks like an unknown screen, and the start-up tap brings them back.
- The game should be on the main screen and the maintainer should not be playing: a run that starts on a screen it does not know taps the start-up point (`enter` in `pages/main_menu.json`) until a known page shows.
- Say what will be pressed and what it can spend. Spending needs the maintainer's order, and a live test spends once: one sweep, one battle, one purchase.

## Looking at a screen (no spending)
Use `tools/live_tour.py` instead of ad-hoc scripts:
```
ocr/.venv/Scripts/python tools/live_tour.py STEPS.json --out captures/tour [--page new_page.json] [--template assets/ko/new.png]
```
- A run goes to the main screen first, so the steps start with the route (`goto`), and one tour covers one visit.
- A click at a fixed point needs `on_page` or `require`; the tool refuses the tour otherwise. On the wrong screen the same point is another button.
- The tool ends every tour with `return_home`. A screen the data does not know yet needs a page file (`--page`), or the next run starts lost.
- Read `captures/tour/sheet.png` first, then single frames. A frame shows the nickname and resources: frames stay in `captures/`, and nothing from them goes into a tracked file except a cropped template without personal data.
- Never press a button that buys, sweeps or confirms during a tour. Opening a confirmation dialog and closing it with back is the limit.

## Templates
- Cut templates with `tools/template_capture.py` (batch mode: `--crop x,y,w,h --task ... --element ...`) from a frame saved by `maba_cli grab` or the tour.
- Leave out what changes: counters, dates, season numbers, days left, red dots, animated background.
- Check that it is unique before using it as an anchor:
  ```
  ocr/.venv/Scripts/python tools/replay_frames.py "captures/**/*.png" --template NAME.png --roi x,y,w,h
  ```
  Every match should be the intended screen and the best other score should stay well below the threshold.
- Matching ignores brightness: a dimmed or greyed element, and anything under a dark overlay, still matches. Tell such states apart with a colour condition, and measure the colour on saved frames of both states.
- A page title can match through a result window drawn over the page. Wait for the overlay to go, not for the title.

## Verifying a task
1. `python tools/check_all.py` (add `--build` after C++ changes).
2. Run the task alone: `maba_cli run --tasks <id>`, with `tools/record_frames.py` beside it when the result needs a look.
3. Run it again in the state it leaves behind (already claimed, nothing to buy, no tickets): it has to end as succeeded or skipped without opening a confirmation.
4. Report what was pressed and spent, what was verified in the game, and what was only checked on saved frames. A state that could not be produced (a reward not yet available, a campaign not running) goes into `docs/ROADMAP.md` as open.

## When the maintainer says something is wrong
Stop the run first (`maba_cli` and the recorder), then look at the frames and the log. Say plainly whether the clicks came from the program under test or from the check itself.

## After a game update
Screens move. Visit every page a task uses (`goto` each page in one tour), compare the frames with the anchors (`replay_frames.py` prints the page and popups per frame), and re-measure fixed points and regions on the new frames. Replace local fixtures in `assets-local/fixtures/` that show the old screen, and note the change with its date in `docs/notes/game-structure.md`.
