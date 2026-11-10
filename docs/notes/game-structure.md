# Game structure

What is known about the game itself, seen in the Korean Steam client unless noted. Keep facts a later task will need and the code does not show. Mark what was not seen in the game with **verify**. Values must hold for every player: no personal data, no numbers of one account. Options: `game-options.md`. Page data: `pages/*.json`.

## Client
Window class `UnityWndClass`, title `Blue Archive`, 16:9 client area. BitBlt, PrintWindow and `SendInput` mouse input all work. Input right after a page change can be ignored.

## Screens

**Main screen.** Top bar: level and nickname, AP, credits, pyroxene, friends, mail, menu collection. Left: notice, MomoTalk, attendance, missions, cash shop (labelled "pyroxene purchase" before the 2026-11-10 update). Bottom bar: cafe, schedule, students, formation, social, crafting, shop, recruitment. Bottom right: campaign hub.

- The menu collection holds options, account, equipment, items, attendance, sticker book and title. The title button may allow a new login without closing the game (**verify**).
- After some idle time the interface hides on the lobby; a tap brings it back.

**Campaign hub.** Top row: missions, virtual aptitude test (beta, locked; added 2026-11-10, which made the missions card narrower), story. Then bounty, commissions, scrimmage, total assault, joint firing drill, grand assault, tactical challenge, limit-release showdown. Event banners sit at the top left and rotate when there are several.

**Sub pages.** The top bar shows AP, credits, pyroxene, the options gear and the home button.

| Page | Contents |
|---|---|
| Cafe | Two cafes. Income panel, invitation tickets, students to touch. The wheel zooms; even fully zoomed out the view does not cover the whole room |
| Schedule | 12 locations in a list, then "all schedules" with the rooms, then a room dialog |
| Bounty | 3 locations, then a stage list |
| Commissions | Base Defense (EXP items) and Item Retrieval (credits), then a stage list |
| Scrimmage | 3 academies, then a stage list |
| Tactical challenge | Claim rewards, opponent list, formation, sortie |
| Shop | Tabs: general, pyroxene, Eligma I and II, total assault, grand assault. "Refresh now" costs pyroxene. The general list is longer than the screen |
| Cash shop | Full screen since the 2026-11-10 update. Categories on the left: recommended, special, subscriptions, general, pyroxene, special support; mostly real-money items. The daily free package is under general, daily: its band turns red ("0 purchases left") once claimed |

Stage dialogs differ in size: the one of campaign missions has tabs and sits about 30 px away from the one of bounty and commissions.

Formation titles depend on the mode (for example "commission formation"), so the sortie button identifies the screen.

On a formation screen the Sortie button pulses and shows before the team has loaded; a click at that moment is ignored. A sortie with an empty team shows only a short notice and stays on the screen, with no dialog and no AP spent. Back during a battle opens the pause menu, and a defeat returns the AP.

## Resources

| Resource | Rule |
|---|---|
| AP | The regular cap grows with the account level. Rewards can pass the cap; regeneration above it: **verify**. Held AP stops at 999; the rest waits in the mailbox |
| Schedule tickets | Limit shown in the schedule screen |
| Bounty tickets | 6 |
| Scrimmage tickets | 6; a sweep also costs AP |
| Tactical challenge tickets | 5 |
| Cafe invitation tickets | Available again after a timer |

Sources of AP: cafe income, mail, mission rewards (100 from a daily mission), circle attendance (10, through the mail), the daily free package (10, with 10,000 credits).

Sweep costs seen: commission stage 14, 40 AP; campaign 30-3, 10 AP; one scrimmage sweep, 15 AP. Commissions have no tickets; no daily limit was seen (**verify**).

Sweeping needs a 3-star clear.

Claim-all buttons turn grey when nothing is left; the grey state still scores 0.87 to 0.90 against the active template, so colour or a threshold of 0.95 tells them apart.

## Daily cycle
The server resets at 04:00 KST (19:00 UTC). Mission deadlines read 03:59:59.

Login order on the first login of the day: logos, notices on the title screen, title, loading, event countdown, attendance reward, attendance board, main screen, then the web news board a few seconds later.

Later logins skip the attendance screens. An "expiring items" notice can follow the news board. It shares its title with other dialogs and is not a popup yet: a confirm click could hit a purchase dialog.

## Interruptions

| Screen | How it ends |
|---|---|
| Web news board | The X at the top right, at the same place in every window size. Web text does not scale with the window, so it is recognized by its white body |
| AP purchase for pyroxene | Opens by itself when a stage costs more AP than there is. Always cancelled |
| Pause menu | Opens with Esc in a battle. Continue goes on; its restart and give-up questions are cancelled |
| Defeat | Confirm, then the AP comes back and a hint follows; confirm leads to the formation |
| Memorial lobby animation | Esc opens a skip question (`game-options.md`) |
| Relationship rank up | A tap; Esc does nothing. Seen after a schedule. In the cafe **verify** |
| Reward and sweep results | Popups, see `popups/` |
| Quit question | Esc on the main screen, or `WM_CLOSE`. Its text looks like the skip question |

## Events
Banners at the top left of the campaign hub, newest first. There are two kinds:

- Story events have Story and Quest tabs; some also have a Challenge tab, which moves the Quest tab (2026-11-10 event: two tabs). First visit: a play guide opens, and a story stage that gives a student shows its introduction screen.
- Other events (seen: a settlement event) have a shop and links to missions and commissions, with no stages of their own. AP spending skips them.

An event whose play period ended still opens and says so.

**Pyroxene purchases.** AP: the + beside AP on the main screen sells 120 AP (30 pyroxene at first, 20 a day); AP stops at 999. Shop "refresh now" (bottom right of the shop) renews the purchase counts, not the items (40 pyroxene at first, 3 a day). Both ask before spending.

## Campaigns
A campaign raises the rewards of one kind of content for a limited time, usually for about a week. Public sources and past announcements; no official schedule for 2026 was found.

| Content | Seen as |
|---|---|
| Normal missions | 2x, 3x |
| Hard missions | 2x, 3x |
| Commissions | 2x, 3x |
| Bounty | 2x, 3x |
| Scrimmage | 2x |
| Schedule (lessons) | 2x, 3x |
| Account EXP | 2x, often on weekends |

Past notices left out relationship points, area EXP, bounty coins and first clear rewards. Fan calendars are predictions and differ from what happens.

In the game a "campaign running" bubble shows on the main screen and a badge on the card in the campaign hub (seen on the missions, bounty and scrimmage cards; the scrimmage badge is drawn slightly smaller). Inside bounty and scrimmage a "campaign running" box states the multiplier, e.g. 3x (+200%) rewards, and the days left. Reading the badge is more reliable than a calendar.

**Batch sweep** (mission page, bottom left): 7 preset tabs that players rename, so they are told apart by position. A preset lists checked Normal and Hard stages (only 3-star stages can be checked); one count applies to every checked stage. Hard stages allow 3 runs a day each, so MAX stops at the runs left; an empty preset or no runs left shows 0 and a grey button. The sweep asks "sweep details" with the total AP, then shows a "sweep done" result.
