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
| Rocker press, held 0.5 s | **Undo**: cursor steps back one slot and clears it (like backspace). Keep holding to undo another rock every 0.5 s; stops after one error buzz when nothing is left. |
| Rocker press, short | Nothing on its own. Two presses within 0.4 s: in coach mode, switch page; in league and competition, change your own position (lead → 2nd → 3rd → skip → lead, remembered). |

In competition mode, rocker up moves the player cursor to the next teammate and down to the previous one (wrapping round). There is no undo.

In league mode the rocker works on one half of the screen at a time, the **focus**, shown by a bold border: your **rocks** (up/down = made/missed, the default at switch-on) or the **players** (up = next player, down = previous, for a rock nobody timed). **Hold rocker up 0.5 s** to focus the players, **hold rocker down 0.5 s** to focus your rocks. So that a hold can be told from a press, league acts on a short up/down press when it's let go.

Timing clicks are timestamped in the interrupt handler, so screen activity never affects timing accuracy.

### Start-up modes

What the rocker is doing while the side button switches the device on picks the mode for that game:

| Switch on with | Mode | Screen | Rocker |
|---|---|---|---|
| Side button only | **Competition** | Times only, full width, the history in a column per teammate, with a cursor on the player about to throw; a black badge strip at the bottom shows the stoprock logo and `COMPETITION` / `STOPWATCH ONLY`, for an umpire | Moves the cursor; double press: your position |
| Side button + rocker **down** | **League** | Section 8: your teammates' times by player, your own 16 rocks and % | Made / missed / undo, or next / previous player (hold up / down to switch); double press: your position |
| Side button + rocker **up** | **Coach** | Two pages (section 8a): the whole team's 64 rocks | Made / missed / undo; double press switches page |

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
- The stopwatch icon on screen is the "timing in progress" sign. The green LED is not used (the case covers it).

## 5. Shots

- League: 8 ends × 2 rocks = 16 slots, filled in order. Coach: 8 ends × 4 positions × 2 rocks = 64 slots, in throwing order (lead, lead, second, second, third, third, skip, skip). Competition: no shots.
- The **cursor** (the slot's own circle drawn bold, 3 px, so it never reaches past its slot) sits on the slot the next made/missed entry will fill. At the start of a game it is on end 1, rock 1. When all slots are filled the cursor disappears.
- Up/down with all slots filled, or undo with nothing to undo: error buzz, no change.
- **Percentage** = made ÷ rocks logged so far this game. Empty slots don't count. Coach mode shows one per position.
- Each game is stored in the databank (section 6) with every rock's result.

## 6. Games and the databank

- **Every switch-on starts a new game.** Light sleep between stones doesn't restart the chip, so it never splits a game. (If the device switches itself off after 5 minutes idle, the next switch-on is a new game.)
- A game is written to flash only once it has a shot or a time in it, so switching on and off doesn't leave empty games.
- The **databank** keeps every game in its own flash partition: start time, shots (made/missed in order), every timed stone's back-to-hog and hog-to-hog times, and the battery charge at switch-on and switch-off (the battery drain log). Up to 500 games; when full, the oldest is dropped. Each game has a number that is never reused.
- A companion app reads new games and acknowledges them; nothing is deleted on acknowledgement. Format and commands: [`DATABANK.md`](DATABANK.md).
- The clock is kept by the BM8563 real-time clock and set by the companion app (separate project), or over the serial console for now. Games recorded before the clock is set are marked so; their times are relative.

## 7. Power

| State | Entered when | Woken by |
|---|---|---|
| Awake | Any press | — |
| Light sleep | Whenever nothing is happening: straight after each timing click once its beep and any screen update are done (including between the clicks of a stone), and 2 s after a rocker press | Any button (the waking press counts as a normal press and is timed from the moment of waking), or a timer: ghost clearing, a stone's 20 s timeout, the clock audit |
| Off | 5 min with no press, or side button held 3 s | Side button held |

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
| Teammates' times | y 49–158 | A column per teammate in each half, headed with their position (a lead sees `2nd`, `3rd`, `Skip`); the player about to throw has the highlighted (inverted) header. Each column lists that player's stones newest first, 7 rows of 14 px, in drawn 7-segment digits (12 px high, 6 px wide, a narrow "1") |
| Your rocks | x 0–135, y 159–199 | Like a scoreboard: ends 1–8 across (drawn digits, 9 px), rock 1 over rock 2 (radius 5); ✓ made, ✗ missed, empty circle not thrown; bold ring = cursor on your next rock |
| Percentage | x 136–199, y 161–199 | Your shots made, DSEG7 16 px digits, % sign drawn to match. A low battery puts a white-on-black `Batt low` bar above it until charged. |

**The player cursor** works as in competition (section 8a): each timed stone is filed under the highlighted player, and after their second rock the cursor moves to the next player. League also knows when you throw: **logging your own shot puts the cursor on the player after you** (as lead, the 2nd; as skip, the lead of the next end), so the cursor is back in step every end even if a stone was missed or an extra click got in. For a rock nobody timed (a teammate called normal, say), move the cursor yourself: hold rocker up to focus the players (a bold border round the times), press up for the next player or down for the previous one, then hold rocker down to go back to your rocks (the border moves to them). Your position is changed with a double press of the rocker centre.

There are no rotating tips: the controls are simple enough, and the help page shows them. One tip remains: for 5 s after switching on (counted from when the screen is up), a box over the times area reads "You are LEAD" (SECOND, THIRD, SKIP), then "Double click rocker to / change your position" and, in league, also "Long hold rocker to / change focus players/rocks". Competition leaves out the focus part (it has no focus); coach shows no tip. The box shows again for 5 s whenever you change position. Your position is kept between games and through power-off. If a stone is being timed when the 5 s are up, the box goes with that stone's next screen update.

## 8a. Competition and coach screens

**Competition.** Labels y 0–12, newest result y 13–47 (DSEG7 25 px), split at x 92 (the H2H half is wider, as its times have a digit more). Below, a column per **teammate** in each half: you can't time your own rocks, so a lead's columns are `2nd`, `3rd`, `Skip`, a third's are `Lead`, `2nd`, `Skip`, and so on. Each column lists that player's stones newest first, 8 rows of 14 px, in drawn digits (12 px high, 6 px wide, a narrow "1"); the newest stone overall is also shown large at the top. The **cursor** is the highlighted (inverted) header of the player about to throw: each timed stone is filed under that player, and after their second rock the cursor moves on to the next player by itself (from the skip back to the first, for the next end). Rocker up/down moves it by hand. A double press of the rocker centre changes your own position (lead → 2nd → 3rd → skip → lead), which the device remembers. The device keeps the last 25 stones for the screen, always at least a screenful of each player. Every switch-on starts with the cursor on the first column. The databank records who threw each stone. The badge strip fills the bottom 26 px, white on black: the stoprock logo on the left (106 × 22 px) and `COMPETITION` / `STOPWATCH ONLY` right-aligned. A low battery replaces `STOPWATCH ONLY` with `Battery low`.

**Coach, page 1 (timer priority).** The competition layout's times down to y 150, then the previous and current end: four columns (L, 2, 3, S), each headed with that position's running percentage, two rows of rocks (radius 5) with the end number on the left. Two small squares top right show which page is up.

**Coach, page 2 (rock priority).** The newest times only (DSEG7 16 px) in y 13–33, position names, all 8 ends × 4 positions × 2 rocks, and along the bottom each position's percentage with a small pie.

## 9. Help page (shown while off)

stoprock logo (176 px), then Spleen 6×12:

```
 [START] hold side button, plus:
     nothing     = competition mode
     rocker down = league mode
     rocker up   = coach mode

 [TIMING] use top or side button

 [SHOTS] up/down = made/missed
            hold = undo last shot
 ─────────────────────────────
 Last game 9/12 shots made (◔) 75%
 ~11 games battery left   [▭] 84%
```

`[START]`, `[TIMING]` and `[SHOTS]` are white-on-black labels; every `=` sits in one column. A competition game's row reads "Last game N stones timed". The last rows show the most recent game, with its shot percentage beside a small pie, and the battery charge at switch-off beside a battery icon; the two percentages are right-aligned. Once at least one game has been completed, the battery row starts with an estimate: "~N games battery left" (from the average drain of the last 10 completed games of 20 minutes or more, rounded down, less one game kept in reserve), or "Charge before next game" when only the reserve is left. The reserve is there because the charge reading is an estimate from the battery voltage, least reliable near empty: the device would rather say charge one game early than run out mid-game.

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
