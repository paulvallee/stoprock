# stoprock: device specification

A pocket curling stopwatch with a shot-percentage tracker. Version 0.3 (design agreed; firmware in `firmware/`, first runs on a CoreInk in November 2026).

Mockups: [`mockups/screens.png`](mockups/screens.png) (off page, league screen, low battery), [`mockups/modes.png`](mockups/modes.png) (the three start-up modes) and [`mockups/rocker-storyboard.png`](mockups/rocker-storyboard.png). Regenerate them with `python tools/mockups/render.py`.

## 1. Purpose

A sweeper times stones and judges their speed at a glance, and a player tracks their own made and missed shots over a game and a season. Speed of reading matters most: the newest time must appear as fast as the screen allows and always in the same place.

## 2. Hardware

Prototype base: **M5Stack CoreInk** (current revision). A custom, branded enclosure comes later and must hide the base product.

| Item | Detail |
|---|---|
| Chip | ESP32-PICO-D4, 4 MB flash |
| Screen | GDEY0154D67, 1.54", 200 × 200, 1-bit black/white, partial refresh 0.26s, full refresh 2s |
| Buttons | Top button, side power button, 3-way rocker (up / press / down), reset (not used) |
| Other | Passive buzzer, green LED, BM8563 real-time clock, 390 mAh battery |
| Rated temperature | 0–60 °C |

**Pins (from M5Stack's older CoreInk library; on a real CoreInk the screen, the top, side and rocker buttons and the power hold work as listed; buzzer and LED not checked yet)**

The top and side buttons need the chip's internal pull-ups. Power hold is held through light sleep (`gpio_hold_en`), as a released latch is thought to be why the first build lost its USB port at its first sleep (not proved: the port has also dropped once since, with the board running on). Waking on a button overwrites its interrupt type with a low level, so the timing buttons' falling edge is restored after every sleep.

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
| Side button, held | Power on (from off), in the mode the rocker picks (below). Power off when held 3s while on (to confirm against the hardware's own power-button behaviour); from 0.5s into the hold a box reads "Keep holding / to turn off" (where the undo one goes). When it shows, that press's timing click is taken back (the hold is a power-off, not a click): a stone it started is cancelled and its stopwatch goes, a B2H it logged goes with its stone, an H2H it logged goes (the stone keeps its B2H if it had one). Let go before 3s and the device stays on. After 3s the off page is drawn and the power goes once the button is let go. On USB, which keeps the board powered, it then sleeps until the side button is pressed, and restarts. |
| Rocker up | Log a **made** shot at the cursor |
| Rocker down | Log a **missed** shot at the cursor |
| Rocker press, held 1s | **Undo** the newest entry, a time or a shot, whichever came last (like backspace), in every mode; a stone being timed is cancelled first (with its B2H, if it has one). The throw order steps back with it. From 0.2s into the hold a box reads "Keep holding / to undo"; it goes with the undo, or when you let go. Keep holding to undo another entry every 1s; stops after one error buzz when nothing is left. |
| Rocker press, short | Two presses within 0.4s switch Coach pages or change your own position in League and Competition (lead → 2nd → 3rd → skip → lead, remembered). A single centre tap in Coach shows the page hint. |

In competition mode, short rocker presses move the player cursor on release: up goes left and down goes right, wrapping round. **Hold up** to move the newest time left, as in league. **Hold down** for 0.7s to enter scroll mode. In scroll mode, tap down for older times or up for newer times, one split row at a time. Every column steps back by the same number of splits; a column with fewer splits runs out (its rows go blank) rather than holding its oldest. Leaving scroll mode puts the cursor back on whoever throws next. The latest result stays visible above the history. Hold up for 0.7s or press either timing button to return to the latest rows. A timing press also records its usual stopwatch click. Holding the rocker centre undoes the newest entry.

In league mode the rocker works on one half of the screen at a time, the **focus**, shown by a bold border: the **columns** of times (up moves left, down moves right, wrapping round; the default at switch-on) or your **rocks** (up/down = made/missed). **Hold down twice, releasing between holds**, to go through history to your rocks. Logging a rock, holding up, or using a timing button returns to the columns: while a stone is timed, the rocker works on the times. When the stone is done, the focus goes back to your rocks if the throw order says you've thrown since your last logged shot (exactly one shot owed: if you aren't logging shots, it leaves the focus alone). When your own throw is due, the focus moves to your rocks by itself (section 8). On the columns, **holding rocker up moves the newest time**: "Moving time" shows after 0.35s (a press to move the cursor is quicker, so it never shows for one), after 1s the time jumps one column left (the way a press of rocker up moves the cursor), and again every 1s (wrapping from the first column to THEM), and it stays in the column it's in when you let go. So does the cursor (the highlighted header), until the next stone starts; then it goes to whoever the throw order says is throwing. From 0.2s into such a hold (when there's a time to move), a box reads "Moving time", over the bottom of the screen (your rocks; competition: the badge strip; coach: the rocks on either page), clear of the columns, until you let go. So that a hold can be told from a press, league acts on a short up/down press when it's let go.

In League and Coach, **hold down for 0.7s** on the timing columns to enter scroll mode. The vertical scrollbar appears on the left. Tap down for older times or up for newer times. Another hold down, after releasing, moves to the made/missed rocks. Hold up or press either timing button from history or rocks to return to the latest rows. A timing press still registers its usual click. Competition has no shot log, so hold down in scroll mode stays in history.

Timing clicks are timestamped in the interrupt handler, so screen activity never affects timing accuracy.

### Start-up modes

Switching on with the side button alone shows the **mode menu**: the 176 px logo stays fixed above a scrollable window with three visible cards. Each card shows a mode and a short description. The last-used mode is selected. Rocker up/down moves the selection and wraps at the ends. Press the rocker centre to open the selected mode. To skip it, hold the rocker while switching on (middle competition, down league, up coach). At the menu the side button held 3s still switches off, and 5 minutes untouched switches off too.

**Tutorials** in the mode menu opens a second picker headed Tutorials. Choose Basic, Competition, League or Coach. Use rocker up/down to go back or forward through the illustrated cards (Basic 8, Competition 11, League 12, Coach 12: one idea a card; timing first, then reading the screen and the rocker, then power), and centre to return to the Tutorials picker; up on the first card or down on the last also returns to it. The cards are drawn by `tools/tutorials_claude/build.py` and stored whole in the firmware (`tools/tutorials.py`). Choose Back to return to the mode menu.

| Switch on with | Mode | Screen | Rocker |
|---|---|---|---|
| Side button + rocker **middle** (or choose it in the menu) | **Competition** | Times only: three player columns at a time, with a cursor on the player about to throw; a black badge strip at the bottom shows the stoprock logo and `COMPETITION` / `STOPWATCH ONLY`, for an umpire | Up moves left, down moves right; hold up to move the newest time, hold down to enter scroll mode; double press: your position |
| Side button + rocker **down** (or the menu) | **League** | Section 8: your teammates' and the other team's times, your own 16 rocks and % | Up moves left, down moves right; hold up to move the newest time; hold down for history, then hold down for made / missed; undo; double press: your position |
| Side button + rocker **up** (or the menu) | **Coach** | Two pages (section 8a): times by position and THEM, the whole team's 64 rocks | As in League, up moves left and down moves right; hold up to move the newest time, hold down for history, then hold down for made / missed; undo; double press switches page |
| The menu only | **Basic** | Times only, as big as they go: a row a stone (newest first, 4 rows), B2H black on white on the left, H2H white on black on the right, no players; the badge strip as in competition, reading `BASIC` | Down / up scroll to older / newer stones, a row a press; hold the middle 1s to undo |

- The menu starts on the mode used last; competition the first time, because it is what a tournament allows: World Curling's rules (R11(c)) limit stopwatches to giving time data only, and forbid electronic communication. Competition mode shows and records times only, and never uses the radio (the radio isn't used in any mode yet).
- The mode lasts until the device switches off; every switch-on is a new game in the mode chosen then. The databank records each game's mode.
- Coach mode will be unlocked by the paid companion app ([`PRO_MODE.md`](PRO_MODE.md)); until the licence check exists it is open to all.
- The rocker is read once, when the firmware starts (about a second after the side button is pressed): hold it before pressing the side button and keep holding until the screen changes.

## 4. Timing

A stone gives at most one **B2H** (back line, or tee line, to hog line) followed by at most one **H2H** (hog line to hog line). On screen these are the Back2Hog and Hog2Hog columns. Gaps are measured between consecutive clicks.

| State | Event | Result |
|---|---|---|
| **Idle** (usually asleep) | Click | Start a new stone; record the time; stopwatch icon on screen |
| **Started** | Click under 1s later | Double press: restart timing from this click |
| | Click 1s to under 7s later | Log **B2H** → *Waiting for H2H* |
| | Click 7–20s later | Log **H2H** on a new row (B2H blank) → *Done* |
| | No click for 20s | Abandon the stone (nothing logged) → *Idle* |
| **Waiting for H2H** | Click under 5.5s after the B2H click | Ignore it (error buzz); keep timing from the B2H click |
| | Click 5.5–20s after the B2H click | Log **H2H** on the same row → *Done* |
| | No click for 20s after the B2H click | Keep the B2H-only row → *Idle* |
| **Done** | — | Sleep at once → *Idle* |

- After an H2H the stone is finished, so the next click always starts the next stone, however soon it comes (an opposing team may throw quickly).
- The window limits (1s double press, 7s B2H, 5.5s shortest H2H after a B2H, 20s) are settings, with these values as defaults. The double-press limit is 1s, not 1.5s, because B2H may be timed from the tee line: tee-to-hog is about 0.5s shorter than back-to-hog, so a peel is about 2s and big weight can approach 1.6s. The B2H limit is 7s: back-to-hog stays under about 4.8s for a slide delivery even for a light guard on fast ice, but a stick delivery can take over 5.5s. After a B2H, though, an H2H can be as short as 5.5s (a big-weight peel can cross hog to hog in about 6s), so that limit stays at 5.5s. Only a stone timed hog to hog alone (two clicks) with 5.5–7s between them is ambiguous: it reads as a B2H, closed without an H2H, the price of not cutting stick deliveries short.
- Every result is also saved to the game databank (section 6).
- The stopwatch icon on screen is the "timing in progress" sign, drawn where the awaited time will appear: in the Back2Hog box after the first click, in the Hog2Hog box once a B2H is logged. If 7s pass after the first click with no second, the time can only be an H2H (the first click was at the near hog line), so the stopwatch moves to the Hog2Hog box then (one screen update; the device wakes from sleep for it). A double press puts it back in the Back2Hog box. The green LED is not used (the case covers it).

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
| Light sleep | Whenever nothing is happening: straight after each timing click once its beep and any screen update are done (including between the clicks of a stone), and 2s after a rocker press | Any button (the waking press counts as a normal press and is timed from the moment of waking), or a timer: ghost clearing, a stone's 20s timeout, the clock audit |
| Off | 2 hours with no press, or side button held 3s | Side button held |

- On switching off, the screen gets one full refresh to the **off page** (section 9), which the e-ink keeps with no power.
- The screen controller stays awake (not in its deep sleep) during a game so updates start immediately; it goes into deep sleep only at power-off.
- The chip runs at 80 MHz, plenty for this app and roughly half the current of full speed.
- **Sleeping while a stone is timed** (setting `sleeptiming`, default on). In light sleep the main crystal stops and the chip's less precise backup clock counts the time asleep, so a 15s H2H spent asleep is only as accurate as that clock. The **clock audit** measures it during real use: every 10 minutes, and at switch-off, it pairs a tick of the real-time clock (which has its own crystal) with the chip's timer, and reports the backup clock's error and the error it implies for a 15s H2H (serial command `audit`, last 8 switch-ons kept). If the error would show in the hundredths, turn the setting off (`sleeptiming off`): the chip then stays awake from a stone's first click until it is done.
- Expected use over a 2-hour, 128-stone game (`tools/game_sim.py`): awake about 3% of the time, about 2% of the battery (estimated currents; to be measured). With sleeptiming off: about 40% and 5%.
- Charge at room temperature only: lithium batteries must not be charged below 0 °C.

## 8. League screen

200 × 200 px, 1-bit. One page.

| Area | Position | Content |
|---|---|---|
| Newest result | y 0–44 | DSEG7 Classic Bold 37 px, square decimal point, centred in its half, split at x 92. No labels: B2H black on white on the left, H2H **white on black** on the right, as in the columns (an H2H of 20.00s, too wide at 37 px, is drawn at 33). Blank cell if that split wasn't timed. |
| Times by player | y 46–158 | Three visible columns across four ordered players: your three teammates in throwing order, then `THEM`. Each shows four recent splits in large digits. Back2Hog is black on white and Hog2Hog is white on black. The active header is highlighted and the 20 px THEM heading distinguishes the other team. A scrollbar above the rocks marks the visible columns. |
| Your rocks | x 0–150, y 159–199 | Like a scoreboard: ends 1–8 across (drawn digits, 9 px), rock 1 over rock 2 (radius 5): filled made, a dot in the ring missed, an empty ring not thrown; bold ring = cursor on your next rock |
| Percentage | x 151–197, y 161–197 (inside the focus border, which goes round the whole bottom, rocks and %, when the rocks are focused) | Your shots made, DSEG7 16 px digits, % sign drawn to match. A low battery puts a white-on-black `Low bat` bar above it until charged. |

**The player cursor.** Each timed stone is filed under the highlighted column. The cursor follows the throw order by itself (`firmware/lib/stoprock_core/src/throw_order.cpp`): an end is 16 throws, the teams alternating, each team's lead throwing two, then the second, third and skip. The device keeps track of where in the end the game is and puts the cursor on whoever throws next, skipping your own throws (you're throwing them):

- **Time passing** counts throws nobody timed: from the start of the last stone (or your logged shot, taken as 20s after your throw), counting throws from their expected times. Each throw's expected time depends on who throws it: lead 45s, 2nd 50s, 3rd 52s, skip 100s from the previous stone's start, 15s more before the first skip stone of an end (skip and vice swap ends) and 60s more before the first stone of the next end (`ThrowOrder::kIntervalS`, `kSkipSwapS`, `kEndBreakS`): about 16.7 minutes an end. These come from one real end timed in full (23 Nov 2026), the end break still a guess; `tools/analyze_games.py` reports each position's real gaps from timed games. A throw counts as missed (untimed) only once the gap is 75% of the way from its expected start to the next one's (`kMissedThrowPct`), not halfway: a slow throw, a skip thinking, is far more common than a missed one, which nearly doubles the gap. Between stones the cursor follows the clock, moving on each time the count goes up, one screen update each time it changes. When a stone starts, the cursor is put right for that moment. In competition, with no shots logged, a 2nd who times the lead's stones and nothing else sees `THEM` until about 5 minutes after the lead's last stone (THEM, you, THEM, you, THEM), then the 3rd.
- **Your logged shots** pin it down: your rock 1 or 2 of the end, in the end your shot log says. Logging one puts the cursor on whoever throws after you.
- **Who throws first** in an end isn't known. The guess is that it swaps every end (the team with the last rock usually scores, and the scoring team throws first next end); the first stone filed in an end can change it, and within an end it changes only on strong evidence.
- **Corrections teach it.** If the cursor is wrong, press up or down to put it right before or while timing: the stone goes where the cursor is, and the order moves on from there. If a time landed in the wrong column, hold rocker up to move it (section 2): the order is redone from that column.

- **Your throw moves the focus to your rocks.** When your rock is the next throw, the focus moves to your rocks by itself 30s after the stone before yours started (about 13s after it's done, if it was timed), so when you're back from throwing, rocker up/down logs your shot straight away. If the stone before yours goes untimed, it happens one throw (~56s) later. It doesn't happen while a stone is being timed, or once you've moved the cursor or the focus yourself; starting the stopwatch brings the focus back to the columns, and if you threw before timing that stone, it returns to your rocks once the stone is done.

The cursor stays where you put it with the rocker until a stone is filed. Your position is changed with a double press of the rocker centre (that starts the order afresh).

There are no rotating tips. Competition and League open with a position card over the history. It names your position and points to the rocker. Double-click the rocker centre to change position and show the card again. The card clears on any button press or after five seconds on screen; for the rocker centre, once the double-click window (0.4s) has passed, so a double click changes position without the card blinking off and on. The button still performs its usual action. Your position is kept between games and through power-off. Coach has no switch-on tip. The hold hints ("Keep holding / to undo", "Keep holding / to turn off", "Moving time", and coach's "Double click / to change page") are bold Spleen 12×24.

## 8a. Competition and coach screens

**Competition.** Newest result y 0–44 as in league (DSEG7 37 px, centred, H2H white on black, no labels), split at x 92 (the H2H half is wider, as its times have a digit more). Below, the history has four player columns: your three **teammates** in throwing order (you can't time your own rocks, so a lead's columns are `2nd`, `3rd`, `Skip`, a third's are `Lead`, `2nd`, `Skip`, and so on) and `THEM`, the other team. Three columns are visible at a time, each 64 px wide with four recent times in 23 px digits. Every header is alike, bold Spleen 12x24, so the rows line up across the columns. Back2Hog is black on white and Hog2Hog white on black. The columns run in **throw order, with THEM in the middle**: on the left the teammate who threw our most recent stone, then THEM, then the next different teammate due to throw (your own throws have no column), worked out from the throw order and the clock. As the end goes on they move: you're 2nd, so during our lead's pair and your own it's Lead | THEM | 3rd, from our 3rd's first rock 3rd | THEM | Skip, and at the start of the next end Skip | THEM | Lead. The rocker steps round them in that order (up left, down right); moving onto the fourth column slides the window that way to show it. A four-part bar above the badge shows the three columns on screen as thick segments and the fourth as a thin one, on the side it's on: press that way to show it. Holding up moves the newest time to the column on its left. The **cursor** is the highlighted (inverted) header of whoever throws next: each timed stone is filed under it, and it follows the throw order by itself, as in league, from the time between stones (competition has no logged shots to pin it, so it relies on the gaps and your corrections). Rocker up/down moves it by hand, and holding rocker up moves the newest time to another column. A double press of the rocker centre changes your own position (lead → 2nd → 3rd → skip → lead), which the device remembers. The device keeps the last 32 stones for the screen, always at least 8 of each player and of THEM. The databank records who threw each stone. The badge strip fills the bottom 26 px, white on black: the stoprock logo on the left (106 × 22 px) and `COMPETITION` / `STOPWATCH ONLY` right-aligned. The second line reads `SCROLL MODE` while browsing, or `Battery low` when the battery is low in the regular view.

**Coach, page 1 (timer priority).** The newest result stays at the top (y 0–44, split at x 100, H2H white on black, with the digits under the page marks). The timing history shows three of the five ordered columns: `Lead`, `2nd`, `3rd`, `Skip` and `THEM`. Each shows four recent splits, with the large THEM heading and a five-part scrollbar. The cursor follows the throw order, and logging the team's rocks helps pin down the order. On the times, short up/down presses move the cursor left/right, holding up moves the newest time left, holding down opens history, and holding down again focuses the rocks. On the rocks, up/down log made/missed and holding up returns to the times. Coach keeps the focus on the rocks while logging successive shots. Pressing either timing button returns the focus to the timing page and registers its timing click. Below the times, the previous and current ends remain visible with four running percentages. Two white marks at the top right show which page is up.

**Coach, page 2 (rock priority).** The newest times only (DSEG7 24 px, H2H white on black, no labels) in y 0–33, position names, all 8 ends × 4 positions × 2 rocks, and along the bottom each position's percentage with a small pie. On page 2 the rocker always works on the rocks (up/down made/missed, a bold border round them), as the page shows no columns; hold rocker up to go back to page 1's times. A double click opens page 2 on the rocks and page 1 on the times. A centre tap shows "Double click / to change page" for 3s, or until the next press.

**Basic.** Competition's times with no players, for reading at a distance: four rows of 43 px, a stone each, newest first, in DSEG7 37 px (an H2H of 20.00s at 33 px), B2H black on white left of x 92 and H2H white on black right of it, 1 px lines between rows. The top row is the stone being timed, with the stopwatch where its time will appear, as in the other modes. The badge strip (bottom 26 px) reads `BASIC` / `STOPWATCH ONLY`. There is no cursor or throw order. A tutorial points to the top and side timing buttons at entry. It closes after 20 seconds on screen or on any button press. A rocker press that closes it does nothing else; a press of the top or side button closes it and also clicks, as with the position card, so the first stone's start isn't lost. The rocker **scrolls**: down a row to older stones, up to newer, through the last 32 kept for the screen (a click at the end beeps the error tone); while scrolled, the badge's second line says which stones show, counting from the newest (`5-8 of 14`), so an old time can't pass for the newest. Starting the stopwatch scrolls back to the top. Holding the middle 1s undoes, as everywhere. The databank records the stones with no player (`null`); `tools/analyze_games.py` skips basic games.

## 9. Off page

The off page uses the [200 × 200 wake screen](../assets/screens/wake-screen-200x200-1bit.png), with its 176 px Stoprock wordmark, “Push here to wake me up!”, and an arrow pointing to the side button at the bottom right. Below the arrow's curve, a battery icon shows estimated games left, “Charge soon” when the one-game reserve is reached, or the charge percentage while the device has too little game history to estimate. The screen refreshes before power is cut, and the e-ink retains the image while the device is off.

## 10. Screen update plan (responsiveness)

The physical update takes about 0.26s whatever the area changed, so the goal is that the screen is **never busy** when a result arrives, and that stray clicks never disturb it.

| Event | Update |
|---|---|
| Startup menu and mode entry | Update all pixels with the fast waveform. Keeping the same waveform across these screens avoids a refresh-mode switch at mode entry or on the first timing click. Timing clicks are captured while a screen update finishes. |
| Start click | Beep. One update: the large row clears and a **stopwatch** appears in the Back2Hog box, where the time will appear (the last stone stays in the history). |
| First result of a stone (B2H, or H2H with no B2H) | Beep. One update: the new value in the large row; after a B2H the stopwatch moves to the Hog2Hog box. |
| H2H after a B2H | Beep. Only the large H2H cell (replacing the stopwatch, which moved there with the B2H). |
| Rocker | Beep. That rock, the cursor and the percentage. |
| Ghost clearing (2s full refresh) | After the **first timed stone** of a game (switching on uses fast updates, which can leave the off page or menu ghosting), then after **every 4th** (a setting, to tune on the real panel): **15s after its H2H click** (time to read it; the next stone usually starts 25-30s after an H2H), or **20s after its B2H click** if it has no H2H. A timer wakes the chip from light sleep for it. Abandoned starts don't count. If the next stone has already started, it waits for that stone to end. |

Rules:
1. During a stone (first click until done or abandoned), only timing updates run. Rocker entries are logged and beep immediately; their screen update waits.
2. If a click lands during a ghost-clearing refresh, its time is still exact. The earliest possible result (1s later) waits at most about 1s for the refresh to finish. In play, a stone can't be delivered within 7s of the previous one crossing the far hog.
3. The screen runs as its own task; input handling never waits for it. Queued updates are merged into the latest state.
4. Every click beeps immediately as confirmation.
5. Target: under 0.3s from click to visible result. To be measured on the device, at room temperature and on the ice.

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
| Spleen 6×12 | Labels | BSD 2-clause |
| Spleen 12×24 | Hold hints | BSD 2-clause |
| Nunito Black | Logo | SIL Open Font License 1.1 |
| Nunito ExtraBold and Bold | Startup menu and tutorials | SIL Open Font License 1.1 |
| Drawn segments | Older results, end numbers | Our own code |

## 14. Out of scope

- Editing or deleting times.
- The companion app: setting the clock and syncing games over Bluetooth. This is a separate project, starting with a Web Bluetooth page, with Android and iOS support later.

## 15. Open items

1. Pin map for the current CoreInk revision: buzzer and LED (and its polarity) still to check.
2. Side power button: hardware behaviour when held, to settle the 3s power-off. On USB, holding it 3s draws the off page, and dropping the latch leaves the board powered. Letting go before 3s leaves it on. A side press then restarts it (seen in the serial log). Switching off and on again on battery was reported to work but has no log (no port then).
3. Measured click-to-visible time, at room temperature and on the ice. So far, on USB at room temperature, the screen update alone took 257-282 ms for a partial and about 1.85s for a full refresh at start-up, but the first fast update after a full refresh took about 520 ms because the driver uses two passes. Startup and mode entry now both use fast updates. The first update when the panel wakes can still need two passes. The result still needs a hardware check.
4. Battery life in real use.
5. Clock audit after a real game: is sleeping while timing accurate enough (section 7)?
6. Trademark check for "stoprock".

## 16. First firmware milestone

An on-device test: press a button, show a time in the large B2H cell, and measure click-to-visible latency, at room temperature and at the rink.
