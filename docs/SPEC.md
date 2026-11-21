# stoprock: device specification

A pocket curling stopwatch with a shot-percentage tracker. Version 0.2 (design agreed; firmware in `firmware/`, not yet run on hardware).

Mockups: [`mockups/screens.png`](mockups/screens.png) (help page, main screen, low battery) and [`mockups/rocker-storyboard.png`](mockups/rocker-storyboard.png). Regenerate them with `python tools/mockups/render.py`.

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
| Side button, held | Power on (from off). Power off when held 3 s while on (to confirm against the hardware's own power-button behaviour). |
| Rocker up | Log a **made** shot at the cursor |
| Rocker down | Log a **missed** shot at the cursor |
| Rocker press, held 0.5 s | **Undo**: cursor steps back one slot and clears it (like backspace). Keep holding to undo another rock every 0.5 s; stops after one error buzz when nothing is left. |
| Rocker press, short | Nothing |

Timing clicks are timestamped in the interrupt handler, so screen activity never affects timing accuracy.

## 4. Timing

A stone gives at most one **B2H** (back line, or tee line, to hog line) followed by at most one **H2H** (hog line to hog line). On screen these are the Back2Hog and Hog2Hog columns. Gaps are measured between consecutive clicks.

| State | Event | Result |
|---|---|---|
| **Idle** (usually asleep) | Click | Start a new stone; record the time; green LED on |
| **Started** | Click under 1.5 s later | Double press: restart timing from this click |
| | Click 1.5 s to under 7 s later | Log **B2H** → *Waiting for H2H* |
| | Click 7–20 s later | Log **H2H** on a new row (B2H blank) → *Done* |
| | No click for 20 s | Abandon the stone (nothing logged) → *Idle* |
| **Waiting for H2H** | Click under 7 s after the B2H click | Ignore it (error buzz); keep timing from the B2H click |
| | Click 7–20 s after the B2H click | Log **H2H** on the same row → *Done* |
| | No click for 20 s after the B2H click | Keep the B2H-only row → *Idle* |
| **Done** | — | LED off; sleep at once → *Idle* |

- After an H2H the stone is finished, so the next click always starts the next stone, however soon it comes (an opposing team may throw quickly).
- The window limits (1.5, 7 and 20 s) are settings, with these values as defaults.
- Every result is also saved to the game databank (section 6).
- The green LED is lit from a stone's first click until it is done or abandoned, as the "timing in progress" sign.

## 5. Shots

- 8 ends × 2 rocks = 16 slots, filled in order.
- The **cursor** (a thick black ring) sits on the slot the next made/missed entry will fill. At the start of a game it is on end 1, rock 1. When all 16 are filled the cursor disappears.
- Up/down with all 16 filled, or undo with nothing to undo: error buzz, no change.
- **Percentage** = made ÷ rocks logged so far this game. Empty slots don't count.
- Each game is stored in the databank (section 6) with every rock's result.

## 6. Games and the databank

- **Every switch-on starts a new game.** Light sleep between stones doesn't restart the chip, so it never splits a game. (If the device switches itself off after 5 minutes idle, the next switch-on is a new game.)
- A game is written to flash only once it has a shot or a time in it, so switching on and off doesn't leave empty games.
- The **databank** keeps every game in its own flash partition: start time, shots (made/missed in order), and every timed stone's back-to-hog and hog-to-hog times. Up to 500 games; when full, the oldest is dropped. Each game has a number that is never reused.
- A companion app reads new games and acknowledges them; nothing is deleted on acknowledgement. Format and commands: [`DATABANK.md`](DATABANK.md).
- The clock is kept by the BM8563 real-time clock and set by the companion app (separate project), or over the serial console for now. Games recorded before the clock is set are marked so; their times are relative.

## 7. Power

| State | Entered when | Woken by |
|---|---|---|
| Awake | Any press | — |
| Light sleep | Whenever nothing is happening: straight after each timing click once its beep and any screen update are done (including between the clicks of a stone), and 2 s after a rocker press | Any button (the waking press counts as a normal press and is timed from the moment of waking), or a timer: the next tip or ghost clearing, a stone's 20 s timeout, the clock audit |
| Off | 5 min with no press, or side button held 3 s | Side button held |

- On switching off, the screen gets one full refresh to the **help page** (section 9), which the e-ink keeps with no power.
- The screen controller stays awake (not in its deep sleep) during a game so updates start immediately; it goes into deep sleep only at power-off.
- The chip runs at 80 MHz, plenty for this app and roughly half the current of full speed.
- **Sleeping while a stone is timed** (setting `sleeptiming`, default on). In light sleep the main crystal stops and the chip's less precise backup clock counts the time asleep, so a 15 s H2H spent asleep is only as accurate as that clock. The **clock audit** measures it during real use: every 10 minutes, and at switch-off, it pairs a tick of the real-time clock (which has its own crystal) with the chip's timer, and reports the backup clock's error and the error it implies for a 15 s H2H (serial command `audit`, last 8 switch-ons kept). If the error would show in the hundredths, turn the setting off (`sleeptiming off`): the chip then stays awake from a stone's first click until it is done.
- Expected use over a 2-hour, 128-stone game (`tools/game_sim.py`): awake about 3% of the time, about 2% of the battery (estimated currents; to be measured). With sleeptiming off: about 40% and 5%.
- Charge at room temperature only: lithium batteries must not be charged below 0 °C.

## 8. Main screen

200 × 200 px, 1-bit. Column edges on 8-pixel boundaries.

| Area | Position | Content |
|---|---|---|
| Column labels | y 0–12 | `Back2Hog`, `Hog2Hog`, `SHOTS` in Spleen 6×12 |
| Newest result | y 13–47, B2H x 0–72, H2H x 72–152 | DSEG7 Classic Bold 25 px, square decimal point. Blank cell if that split wasn't timed. |
| Older results | below, newest first, 15 px rows, up to 8 rows | Drawn 7-segment digits: 12 px high, 7 px wide, 2 px strokes, empty corner pixels, 2 × 2 decimal point. Equal-width digits so points line up. |
| Tip strip | x 0–152, bottom 26 px | Two lines of Spleen 6×12, max 25 characters each |
| Shots | x 152–200 | One row per end (1–8), rock 1 and rock 2 side by side; ✓ made, ✗ missed, empty circle not thrown; ring = cursor. End numbers in drawn segments (9 px, 1 px strokes). |
| Percentage | bottom of the shots column | DSEG7 16 px digits, packed closely, % sign drawn to match. 100% fits. |

**Tips** rotate in this fixed order, one step after each timed stone (a partial update of the strip). The position is kept across power-offs. A low-battery warning replaces the tip, shown white on black, until the battery is charged.

| # | Line 1 | Line 2 |
|---|---|---|
| 1 | Rocker up: made shot | Rocker down: missed |
| 2 | Hold rocker to undo | keep holding to undo more |
| 3 | Back2Hog: back/tee to hog | click far hog for Hog2Hog |
| 4 | Top or side button both | time. Use either one. |
| 5 | Two clicks within 1.5 s | count as one |
| 6 | After an H2H, the next | click starts a new stone |
| 7 | Each switch-on starts | a new game |
| 8 | Off after 5 min idle. | Hold side to switch on |
| Warning | Battery low | Charge after this game |

## 9. Help page (shown while off)

stoprock logo (176 px), then Spleen 6×12:

```
     HOLD SIDE BUTTON TO START

 [TIMING] top or side button
  click back line, hog, far hog
  B2H back or tee to hog line
  H2H hog line to hog line

 [SHOTS]  rocker
  up = made     down = missed
  hold 0.5 s = undo last shot
 ─────────────────────────────
 Off after 5 min idle    [▭] 84%
 Last game 21 Nov   9/12   75%
```

`[TIMING]` and `[SHOTS]` are white-on-black labels. The last line shows the most recent game.

## 10. Screen update plan (responsiveness)

The physical update takes about 0.26 s whatever the area changed, so the goal is that the screen is **never busy** when a result arrives, and that stray clicks never disturb it.

| Event | Update |
|---|---|
| Start click | Beep and LED on. **No screen update.** |
| First result of a stone (B2H, or H2H with no B2H) | Beep. One update: move the previous result into the list, and show the new value in the large row. |
| H2H after a B2H | Beep. Only the large H2H cell. |
| Rocker | Beep. That rock, the cursor and the percentage. |
| Next tip (partial update of the tip strip) | After **every timed stone**: **5 s after its H2H click**, or **20 s after its B2H click** if it has no H2H. A timer wakes the chip from light sleep for it. Abandoned starts don't count. If the next stone has already started, it waits for that stone to end. |
| Ghost clearing (2 s full refresh) | Takes the place of the tip update after **every 4th timed stone** (a setting, to tune on the real panel); it shows the next tip too. |

Rules:
1. During a stone (first click until done or abandoned), only timing updates run. Rocker entries are logged and beep immediately; their screen update waits.
2. If a click lands during a ghost-clearing refresh, its time is still exact. The earliest possible result (1.5 s later) waits at most about 0.5 s for the refresh to finish. In play, a stone can't be delivered within 7 s of the previous one crossing the far hog.
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
| Spleen 6×12 | Labels, tips, help page | BSD 2-clause |
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
