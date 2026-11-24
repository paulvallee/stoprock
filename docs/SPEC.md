# stoprock: device specification

A pocket curling stopwatch with a shot-percentage tracker. Version 0.3 (design agreed; firmware in `firmware/`, not yet run on hardware).

Mockups: [`mockups/screens.png`](mockups/screens.png) (help page, league screen, low battery), [`mockups/modes.png`](mockups/modes.png) (the three start-up modes) and [`mockups/rocker-storyboard.png`](mockups/rocker-storyboard.png). Regenerate them with `python tools/mockups/render.py`.

## 1. Purpose

A sweeper times stones and judges their speed at a glance, and a player tracks their own made and missed shots over a game and a season. Speed of reading matters most: the newest time must appear as fast as the screen allows and always in the same place.

## 2. Hardware

Prototype base: **M5Stack CoreInk** (current revision). A custom, branded enclosure comes later and must hide the base product.

| Item | Detail |
|---|---|
| Chip | ESP32-PICO-D4, 4 MB flash |
| Screen | GDEY0154D67, 1.54", 200 × 200, 1-bit black/white, partial refresh 0.26 s, full refresh 2 s |
| Buttons | Top button, side power button, 3-way rocker (up / press / down), reset (not used) |
| Other | Passive buzzer, green LED, BM8563 real-time clock, 390 mAh battery |
| Rated temperature | 0–60 °C |

**Pins (from M5Stack's older CoreInk library; verify against the current revision before use)**

| Function | GPIO |
|---|---|
| Top button | 5 |
| Rocker up / press / down | 37 / 38 / 39 |
| Side power button | 27 |
| Power hold (must be held high to stay on) | 12 |
| Screen SPI: CS, SCK, MOSI, DC, RST, BUSY | 9, 18, 23, 15, 0, 4 |
| Buzzer / LED | 2 / 10 |

Graphics library: **M5GFX / M5Unified** (has a driver for this screen, `Panel_GDEW0154D67`, 256 ms refresh, selectable update modes).

## 3. Controls

| Input | Action |
|---|---|
| Top button **or** side button (short press) | Timing click. Either works; the user picks whichever is comfortable. |
| Side button, held | Power on (from off), in the mode the rocker picks (below). Power off when held 3 s while on (to confirm against the hardware's own power-button behaviour). |
| Rocker up | Log a **made** shot at the cursor |
| Rocker down | Log a **missed** shot at the cursor |
| Rocker press, held 1 s | **Undo** the newest entry, a time or a shot, whichever came last (like backspace), in every mode; a stone being timed is cancelled first (with its B2H, if it has one). The throw order steps back with it. From 0.2 s into the hold a box reads "Keep holding to undo"; it goes with the undo, or when you let go. Keep holding to undo another entry every 1 s; stops after one error buzz when nothing is left. |
| Rocker press, short | Nothing on its own. Two presses within 0.4 s: in coach mode, switch page; in league and competition, change your own position (lead → 2nd → 3rd → skip → lead, remembered). |

In competition mode, rocker up moves the player cursor to the next column and down to the previous one (wrapping round), and **holding rocker up moves the newest time** right, as in league (below); with no focus to move, **holding rocker down moves it left**. There are no rocks; holding the rocker centre undoes the newest time. As in league, up/down act when let go.

In league mode the rocker works on one half of the screen at a time, the **focus**, shown by a bold border: the **columns** of times (up = next column, down = previous, wrapping round; the default at switch-on) or your **rocks** (up/down = made/missed). **Hold rocker down 0.5 s** to log your rocks; logging one (or holding rocker up 0.5 s) goes back to the columns, and so does starting the stopwatch: while a stone is timed, the rocker works on the times. When the stone is done, the focus goes back to your rocks if the throw order says you've thrown since your last logged shot (exactly one shot owed: if you aren't logging shots, it leaves the focus alone). When your own throw is due, the focus moves to your rocks by itself (section 8). On the columns, **holding rocker up moves the newest time**: after 0.7 s it jumps one column right (the way a press of rocker up moves the cursor), and again every 0.7 s (wrapping round from THEM to the first column), and it stays in the column it's in when you let go. So does the cursor (the highlighted header), until the next stone starts; then it goes to whoever the throw order says is throwing. So that a hold can be told from a press, league acts on a short up/down press when it's let go.

Timing clicks are timestamped in the interrupt handler, so screen activity never affects timing accuracy.

### Start-up modes

What the rocker is doing while the side button switches the device on picks the mode for that game:

| Switch on with | Mode | Screen | Rocker |
|---|---|---|---|
| Side button only | **Competition** | Times only: the history in four columns (your teammates and THEM) as in league, with a cursor on the player about to throw; a black badge strip at the bottom shows the stoprock logo and `COMPETITION` / `STOPWATCH ONLY`, for an umpire | Moves the cursor, hold up to move the newest time; double press: your position |
| Side button + rocker **down** | **League** | Section 8: your teammates' and the other team's times, your own 16 rocks and % | Next / previous column, hold up to move the newest time; hold down for made / missed; undo; double press: your position |
| Side button + rocker **up** | **Coach** | Two pages (section 8a): times by position and THEM, the whole team's 64 rocks | As league: next / previous column, hold up to move the newest time, hold down for made / missed; undo; double press switches page |

- Competition is the default because it is what a tournament allows: World Curling's rules (R11(c)) limit stopwatches to giving time data only, and forbid electronic communication. Competition mode shows and records times only, and never uses the radio (the radio isn't used in any mode yet).
- The mode lasts until the device switches off; every switch-on is a new game in the mode chosen then. The databank records each game's mode.
- Coach mode will be unlocked by the paid companion app ([`PRO_MODE.md`](PRO_MODE.md)); until the licence check exists it is open to all.
- The rocker is read once, when the firmware starts (about a second after the side button is pressed): hold it before pressing the side button and keep holding until the screen changes.

## 4. Timing

A stone gives at most one **B2H** (back line, or tee line, to hog line) followed by at most one **H2H** (hog line to hog line). On screen these are the Back2Hog and Hog2Hog columns. Gaps are measured between consecutive clicks.

| State | Event | Result |
|---|---|---|
| **Idle** (usually asleep) | Click | Start a new stone; record the time; stopwatch icon on screen |
| **Started** | Click under 1 s later | Double press: restart timing from this click |
| | Click 1 s to under 5.5 s later | Log **B2H** → *Waiting for H2H* |
| | Click 5.5–20 s later | Log **H2H** on a new row (B2H blank) → *Done* |
| | No click for 20 s | Abandon the stone (nothing logged) → *Idle* |
| **Waiting for H2H** | Click under 5.5 s after the B2H click | Ignore it (error buzz); keep timing from the B2H click |
| | Click 5.5–20 s after the B2H click | Log **H2H** on the same row → *Done* |
| | No click for 20 s after the B2H click | Keep the B2H-only row → *Idle* |
| **Done** | — | Sleep at once → *Idle* |

- After an H2H the stone is finished, so the next click always starts the next stone, however soon it comes (an opposing team may throw quickly).
- The window limits (1, 5.5 and 20 s) are settings, with these values as defaults. The double-press limit is 1 s, not 1.5 s, because B2H may be timed from the tee line: tee-to-hog is about 0.5 s shorter than back-to-hog, so a peel is about 2 s and big weight can approach 1.6 s. The B2H/H2H boundary is 5.5 s, not 7 s: back-to-hog stays under about 4.8 s even for a light guard on fast ice, while a big-weight peel can cross hog to hog in about 6 s.
- Every result is also saved to the game databank (section 6).
- The stopwatch icon on screen is the "timing in progress" sign, drawn where the awaited time will appear: in the Back2Hog box after the first click, in the Hog2Hog box once a B2H is logged. If 5.5 s pass after the first click with no second, the time can only be an H2H (the first click was at the near hog line), so the stopwatch moves to the Hog2Hog box then (one screen update; the device wakes from sleep for it). A double press puts it back in the Back2Hog box. The green LED is not used (the case covers it).

## 5. Shots

- League: 8 ends × 2 rocks = 16 slots, filled in order. Coach: 8 ends × 4 positions × 2 rocks = 64 slots, in throwing order (lead, lead, second, second, third, third, skip, skip). Competition: no shots.
- The **cursor** (the slot's own circle drawn bold, 3 px, so it never reaches past its slot) sits on the slot the next made/missed entry will fill. At the start of a game it is on end 1, rock 1. When all slots are filled the cursor disappears.
- Up/down with all slots filled, or undo with nothing to undo: error buzz, no change.
- **Percentage** = made ÷ rocks logged so far this game. Empty slots don't count. Coach mode shows one per position.
- Each game is stored in the databank (section 6) with every rock's result.

## 6. Games and the databank

- **Every switch-on starts a new game.** Light sleep between stones doesn't restart the chip, so it never splits a game. (If the device switches itself off after 2 hours idle, the next switch-on is a new game. Two hours is a whole game: someone who times only a stone or two, say in the last end, keeps the game going.)
- A game is written to flash only once it has a shot or a time in it, so switching on and off doesn't leave empty games.
- The **databank** keeps every game in its own flash partition: start time, shots (made/missed in order), every timed stone's back-to-hog and hog-to-hog times, and the battery charge at switch-on and switch-off (the battery drain log). Up to 500 games; when full, the oldest is dropped. Each game has a number that is never reused.
- A companion app reads new games and acknowledges them; nothing is deleted on acknowledgement. Format and commands: [`DATABANK.md`](DATABANK.md).
- The clock is kept by the BM8563 real-time clock and set by the companion app (separate project), or over the serial console for now. Games recorded before the clock is set are marked so; their times are relative.

## 7. Power

| State | Entered when | Woken by |
|---|---|---|
| Awake | Any press | — |
| Light sleep | Whenever nothing is happening: straight after each timing click once its beep and any screen update are done (including between the clicks of a stone), and 2 s after a rocker press | Any button (the waking press counts as a normal press and is timed from the moment of waking), or a timer: ghost clearing, a stone's 20 s timeout, the clock audit |
| Off | 2 hours with no press, or side button held 3 s | Side button held |

- On switching off, the screen gets one full refresh to the **help page** (section 9), which the e-ink keeps with no power.
- The screen controller stays awake (not in its deep sleep) during a game so updates start immediately; it goes into deep sleep only at power-off.
- The chip runs at 80 MHz, plenty for this app and roughly half the current of full speed.
- **Sleeping while a stone is timed** (setting `sleeptiming`, default on). In light sleep the main crystal stops and the chip's less precise backup clock counts the time asleep, so a 15 s H2H spent asleep is only as accurate as that clock. The **clock audit** measures it during real use: every 10 minutes, and at switch-off, it pairs a tick of the real-time clock (which has its own crystal) with the chip's timer, and reports the backup clock's error and the error it implies for a 15 s H2H (serial command `audit`, last 8 switch-ons kept). If the error would show in the hundredths, turn the setting off (`sleeptiming off`): the chip then stays awake from a stone's first click until it is done.
- Expected use over a 2-hour, 128-stone game (`tools/game_sim.py`): awake about 3% of the time, about 2% of the battery (estimated currents; to be measured). With sleeptiming off: about 40% and 5%.
- Charge at room temperature only: lithium batteries must not be charged below 0 °C.

## 8. League screen

200 × 200 px, 1-bit. One page.

| Area | Position | Content |
|---|---|---|
| Column labels | y 0–12 | `Back2Hog`, `Hog2Hog` in Spleen 6×12, split at x 92 |
| Newest result | y 13–47 | DSEG7 Classic Bold 25 px, square decimal point. Blank cell if that split wasn't timed. |
| Times by player | y 49–158 | Four columns, 49 px wide: your three teammates in throwing order, headed with their position (a 2nd sees `Lead`, `3rd`, `Skip`), then `THEM`, the other team. The player about to throw has the highlighted (inverted) header. Each column lists its stones newest first, a time a line, 7 lines of 14 px, in drawn 7-segment digits (12 px high, 7 px wide): Back2Hog black on white, Hog2Hog **white on black**, so a stone timed both ways takes two lines and a column shows its last 3–4 stones |
| Your rocks | x 0–135, y 159–199 | Like a scoreboard: ends 1–8 across (drawn digits, 9 px), rock 1 over rock 2 (radius 5); ✓ made, ✗ missed, empty circle not thrown; bold ring = cursor on your next rock |
| Percentage | x 136–199, y 161–199 | Your shots made, DSEG7 16 px digits, % sign drawn to match. A low battery puts a white-on-black `Batt low` bar above it until charged. |

**The player cursor.** Each timed stone is filed under the highlighted column. The cursor follows the throw order by itself (`firmware/lib/stoprock_core/src/throw_order.cpp`): an end is 16 throws, the teams alternating, each team's lead throwing two, then the second, third and skip. The device keeps track of where in the end the game is and puts the cursor on whoever throws next, skipping your own throws (you're throwing them):

- **Time passing** counts throws nobody timed: from the start of the last stone (or your logged shot, taken as 20 s after your throw), the count whose expected time is nearest. Each throw's expected time depends on who throws it: lead 50 s, 2nd 52 s, 3rd 56 s, skip 70 s from the previous stone's start, 20 s more before the first skip stone of an end (skip and vice swap ends) and 60 s more before the first stone of the next end (`ThrowOrder::kIntervalS`, `kSkipSwapS`, `kEndBreakS`): about 15.5 minutes an end. These are guesses to be tuned from timed games (`tools/analyze_games.py` reports each position's real gaps). Between stones the cursor follows the clock, moving on each time the count goes up (halfway between one throw's expected start and the next), one screen update each time it changes. When a stone starts, the cursor is put right for that moment. In competition, with no shots logged, a 2nd who times the lead's stones and nothing else sees `THEM` until about 5 minutes after the lead's last stone (THEM, you, THEM, you, THEM), then the 3rd.
- **Your logged shots** pin it down: your rock 1 or 2 of the end, in the end your shot log says. Logging one puts the cursor on whoever throws after you.
- **Who throws first** in an end isn't known. The guess is that it swaps every end (the team with the last rock usually scores, and the scoring team throws first next end); the first stone filed in an end can change it, and within an end it changes only on strong evidence.
- **Corrections teach it.** If the cursor is wrong, press up or down to put it right before or while timing: the stone goes where the cursor is, and the order moves on from there. If a time landed in the wrong column, hold rocker up to move it (section 2): the order is redone from that column.

- **Your throw moves the focus to your rocks.** When your rock is the next throw, the focus moves to your rocks by itself 30 s after the stone before yours started (about 13 s after it's done, if it was timed), so when you're back from throwing, rocker up/down logs your shot straight away. If the stone before yours goes untimed, it happens one throw (~56 s) later. It doesn't happen while a stone is being timed, or once you've moved the cursor or the focus yourself; starting the stopwatch brings the focus back to the columns, and if you threw before timing that stone, it returns to your rocks once the stone is done.

The cursor stays where you put it with the rocker until a stone is filed. Your position is changed with a double press of the rocker centre (that starts the order afresh).

There are no rotating tips: the controls are simple enough, and the help page shows them. One tip remains: for 5 s after switching on (counted from when the screen is up), a box over the times area reads "You are LEAD" (SECOND, THIRD, SKIP), then "Double click rocker to / change your position" and, in league, also "Hold rocker down: your rocks" / "Hold rocker up: move last time" / "Hold rocker middle: undo". Competition leaves out the rocks line (it has no rocks). Coach shows "Double click rocker to / switch page", "Hold rocker down: rocks", "Hold rocker up: move last time" and "Hold rocker middle: undo" on page 1. The box shows again for 5 s whenever you change position. Your position is kept between games and through power-off. If a stone is being timed when the 5 s are up, the box goes with that stone's next screen update.

## 8a. Competition and coach screens

**Competition.** Labels y 0–12, newest result y 13–47 (DSEG7 25 px), split at x 92 (the H2H half is wider, as its times have a digit more). Below, the four columns of league (section 8): your three **teammates** in throwing order (you can't time your own rocks, so a lead's columns are `2nd`, `3rd`, `Skip`, a third's are `Lead`, `2nd`, `Skip`, and so on) and `THEM`, the other team. Each column lists its stones newest first, a time a line, 8 lines of 14 px: Back2Hog black on white, Hog2Hog white on black. The **cursor** is the highlighted (inverted) header of whoever throws next: each timed stone is filed under it, and it follows the throw order by itself, as in league, from the time between stones (competition has no logged shots to pin it, so it relies on the gaps and your corrections). Rocker up/down moves it by hand, and holding rocker up moves the newest time to another column. A double press of the rocker centre changes your own position (lead → 2nd → 3rd → skip → lead), which the device remembers. The device keeps the last 32 stones for the screen, always at least 8 of each player and of THEM. The databank records who threw each stone. The badge strip fills the bottom 26 px, white on black: the stoprock logo on the left (106 × 22 px) and `COMPETITION` / `STOPWATCH ONLY` right-aligned. A low battery replaces `STOPWATCH ONLY` with `Battery low`.

**Coach, page 1 (timer priority).** The newest result large (y 13–47, split at x 100), then five columns of times, 40 px wide: `Lead`, `2nd`, `3rd`, `Skip` and `THEM`, each listing its stones newest first, a time a line (6 lines, down to y 150), Back2Hog black on white and Hog2Hog white on black, in narrow drawn digits (12 px high, 6 px wide, a narrow "1") so a time fits a column. The **cursor** (highlighted header) follows the throw order as in league (section 8); a coach logs every one of the team's rocks, in throwing order, so each logged rock pins down where in the end the game is, and the order is surest here. The rocker works as in league (section 2), with a focus shown by a bold border on both pages: on the **times**, up/down move the cursor (next/previous column) and holding up moves the newest time; **hold down** for the **rocks**, where up/down log made/missed, and **hold up** to go back to the times. Logging a rock goes back to the times; starting the stopwatch brings the focus to the times, and when one of the team's stones is done (a rock owed: the throw order counts every team throw a coach logs), the focus goes to the rocks, ready for its result. Below the times, the previous and current end: four columns (L, 2, 3, S), each headed with that position's running percentage, two rows of rocks (radius 5) with the end number on the left. Two small squares top right show which page is up.

**Coach, page 2 (rock priority).** The newest times only (DSEG7 16 px) in y 13–33, position names, all 8 ends × 4 positions × 2 rocks, and along the bottom each position's percentage with a small pie.

## 9. Help page (shown while off)

stoprock logo (176 px), then Spleen 6×12:

```
 [START] hold side button, plus:
     nothing     = competition mode
     rocker down = league mode
     rocker up   = coach mode

 [TIMING] use top or side button
         hold up = move last time

 [SHOTS] up/down = made/missed

 [UNDO] hold centre: time or shot
 ─────────────────────────────
 Last game 9/12 shots made (◔) 75%
 ~11 games battery left   [▭] 84%
```

`[START]`, `[TIMING]`, `[SHOTS]` and `[UNDO]` are white-on-black labels; every `=` sits in one column. A competition game's row reads "Last game N stones timed". The last rows show the most recent game, with its shot percentage beside a small pie, and the battery charge at switch-off beside a battery icon; the two percentages are right-aligned. Once at least one game has been completed, the battery row starts with an estimate: "~N games battery left" (from the average drain of the last 10 completed games of 20 minutes or more, rounded down, less one game kept in reserve), or "Charge before next game" when only the reserve is left. The reserve is there because the charge reading is an estimate from the battery voltage, least reliable near empty: the device would rather say charge one game early than run out mid-game.

## 10. Screen update plan (responsiveness)

The physical update takes about 0.26 s whatever the area changed, so the goal is that the screen is **never busy** when a result arrives, and that stray clicks never disturb it.

| Event | Update |
|---|---|
| Start click | Beep. One update: the large row clears and a **stopwatch** appears in the Back2Hog box, where the time will appear (the last stone stays in the history). |
| First result of a stone (B2H, or H2H with no B2H) | Beep. One update: the new value in the large row; after a B2H the stopwatch moves to the Hog2Hog box. |
| H2H after a B2H | Beep. Only the large H2H cell (replacing the stopwatch, which moved there with the B2H). |
| Rocker | Beep. That rock, the cursor and the percentage. |
| Ghost clearing (2 s full refresh) | After **every 4th timed stone** (a setting, to tune on the real panel): **5 s after its H2H click**, or **20 s after its B2H click** if it has no H2H. A timer wakes the chip from light sleep for it. Abandoned starts don't count. If the next stone has already started, it waits for that stone to end. |

Rules:
1. During a stone (first click until done or abandoned), only timing updates run. Rocker entries are logged and beep immediately; their screen update waits.
2. If a click lands during a ghost-clearing refresh, its time is still exact. The earliest possible result (1 s later) waits at most about 1 s for the refresh to finish. In play, a stone can't be delivered within 7 s of the previous one crossing the far hog.
3. The screen runs as its own task; input handling never waits for it. Queued updates are merged into the latest state.
4. Every click beeps immediately as confirmation.
5. Target: under 0.3 s from click to visible result. To be measured on the device, at room temperature and on the ice.

## 11. Sounds

| Event | Sound |
|---|---|
| Timing click | Short beep |
| Made | High beep |
| Missed | Low beep |
| Undo | Falling two-tone |
| Not allowed (including a click too soon after a B2H) | Error buzz |

## 12. Brand

- Name: **stoprock** (lowercase). Check for an existing trademark before printing it on a product.
- Logo: Nunito Black lettering; both o's are curling rocks seen from the side, with a white band and a handle that rises to the height of the t and k. The device version uses a thicker band so it survives at 1 bit.

## 13. Fonts and licences

| Font | Use | Licence |
|---|---|---|
| DSEG7 Classic Bold | Newest result, percentage | SIL Open Font License 1.1 |
| Spleen 6×12 | Labels, help page | BSD 2-clause |
| Nunito Black | Logo | SIL Open Font License 1.1 |
| Drawn segments | Older results, end numbers | Our own code |

## 14. Out of scope

- Editing or deleting times.
- The companion app: setting the clock and syncing games over Bluetooth. This is a separate project, starting with a Web Bluetooth page, with Android and iOS support later.

## 15. Open items

1. Pin map for the current CoreInk revision.
2. Side power button: hardware behaviour when held, to settle the 3 s power-off.
3. Measured click-to-visible time, at room temperature and on the ice.
4. Battery life in real use.
5. Clock audit after a real game: is sleeping while timing accurate enough (section 7)?
6. Trademark check for "stoprock".

## 16. First firmware milestone

An on-device test: press a button, show a time in the large B2H cell, and measure click-to-visible latency, at room temperature and at the rink.
