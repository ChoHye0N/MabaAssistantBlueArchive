# Game options that affect automation

Read in the Korean Steam client on 2026-11-05 (Options: gear icon at the top right of sub pages). The options differ per player, so MABA must not assume one value. Items marked **verify** were not tested.

Tabs, top to bottom: Game, Graphics, Volume, Title, Language, Controls, Story.

## Impact

| Tab | Option | Values | Effect on MABA |
|---|---|---|---|
| Game | Memorial lobby start animation (메모리얼 로비의 시작 연출) | On, once a day, off | **High.** See below |
| Language | Language | Korean, Thai, Traditional Chinese, English | **High.** Templates, OCR and `strings/` follow the client language (`Language` setting) |
| Graphics | Screen mode | Full screen, window | **High.** Window size sets the source of the frames. Web pages inside the game do not scale with it (news board) |
| Graphics | Render scale | Slider (2.0 here) | **High, verify.** Changes how text is drawn, so template scores may fall |
| Graphics | Post processing, anti-aliasing | On, off | **Medium, verify.** Same as render scale |
| Graphics | Rendering acceleration mode | Standard, compatible, off | **Medium, verify.** Capture may be affected |
| Graphics | Battle letterbox | On, off | **Medium, verify.** Can move the battle HUD. AUTO and speed checks use fixed regions |
| Controls | Shortcut settings | Key binding screen | **Medium, verify.** `Back` sends Esc. If Esc is rebound, back does nothing, which the reaction check reports as a failure |
| Controls | Shortcut icons | On, off | **Medium, verify.** Hides the `Space`, `Esc` and `F` hints on buttons. Templates cut without hints are safe; those not recaptured yet may not be |
| Controls | Game mouse pointer | On, off | **Low, verify.** The pointer may appear in captures |
| Game | EX skill animation | Always, once a day, off | **Medium.** Longer battles; waits are bounded by screen state |
| Game | Skill card position | Left, right | **Low, verify.** Battle HUD layout |
| Game | Cafe memory cleanup | On, off | **Low, verify.** Cafe loading |
| Game | Lobby assistant rule | Always random, random once a day, fixed | **Low.** The lobby picture changes; the menu templates sit on the opaque bar |
| Story | Automatic choice | On, off | **Medium.** Story scenes are skipped, so only stages that show a choice before the skip matter (verify) |
| Story | Main story reward animation auto | On, off | **Low.** Result screens are popups |
| Story | Next scenario auto continue | On, off | **Medium, verify.** Off may show a popup MABA does not know |
| Story | Text speed, AUTO wait time | Four steps each | None: scenes are skipped |
| Title | Title video | 1st to 7th PV, 4.5th PV, random | None: start-up taps do not depend on the picture |
| Graphics | FPS, V-Sync | 60 or 30; on, off | **Low.** Animation timing only |

No effect, for the record: boss restart intro, skill effect icons, EX target markers, same-name students in a team, voice subtitles, tactical challenge anonymity, cafe furniture copy, volume, voice language.

## Memorial lobby start animation
Values: on, once a day, off. Measured 2026-11-05, Steam client at 1920x1080.

| Value | When it plays |
|---|---|
| On | Every time the main screen opens, also when returning from another page; about 18 s |
| Once a day | The first time of the day only; a later login goes straight to the menu |
| Off | Never (**verify**) |

While it plays no menu is visible, so no page or popup matches.

- A tap (top or centre, several times) or Space changes nothing: the menu still takes about 18 s.
- **Esc opens the question "skip?"** with Cancel and OK, and the animation keeps playing behind it. OK ends the animation.
- MABA handles it without knowing the option: on an unknown screen the Navigator presses back, and the popup `lobby_skip` confirms the question. A return to the main screen takes about 12 s. With the value off the path is never taken.
- `lobby_skip` needs its threshold of 0.95: the quit dialog ("quit?") scores 0.83, and confirming that one closes the game.

Not verified: the animation at login, where `EnterGame` taps instead of pressing back, so it waits for the end of the animation.
