"""Model a game against the firmware's rules (firmware/src/app.cpp) to count screen
refreshes, awake time and battery use.

    python3 tools/game_sim.py

Rules modelled (docs/SPEC.md sections 4, 7, 10), with sleep-while-timing on (the default):
- Each click wakes the chip just long enough to confirm the press and beep; a B2H or H2H
  result also waits for its partial update. In between, it sleeps.
- 5 s after each H2H the chip wakes for the next tip: a partial update, or a full
  (ghost-clearing) refresh after every 4th stone. Then it sleeps again.
- A rocker press wakes it, logs the shot (one partial update when no stone is being
  timed) and keeps it awake 2 s.
- Every 10 minutes a clock-audit sample keeps it awake up to ~1 s (0.5 s on average).
- The chip runs at 80 MHz.
- Switching on draws the menu and the mode with fast whole-screen updates (as partials);
  the first ghost-clearing full refresh follows the first stone. Switching off (the off page)
  is one more full refresh, after the game.
"""

GAME_S = 2 * 3600
ENDS, STONES_PER_END = 8, 16
B2H_S, H2H_S = 4.0, 15.0
PARTIAL_S, FULL_S = 0.26, 2.0
STONES_PER_CLEAN = 4
SHOT_AWAKE_S = 2.0
CLICK_AWAKE_S = 0.05          # confirm the press (15 ms) and beep (30 ms)
AUDIT_EVERY_S, AUDIT_AWAKE_S = 600, 0.5
REFRESH_DELAY_S = 5.0
OWN_ROCKS = (0, 2)           # your two rocks in each end (as lead: stones 1 and 3)
SHOT_LOGGED_AFTER_S = 35.0   # you log your rock ~35 s after its back-line click, once it stops
WAKE_OVERHEAD_S = 0.02       # wake-up, run the loop, go back to sleep

# Estimated currents at 3.7 V (to be measured on a real CoreInk), low / typical / high
AWAKE_MA = (15, 20, 30)      # ESP32 at 80 MHz, radio off, mostly waiting
SLEEP_MA = (1.0, 2.0, 3.0)   # ESP32 light sleep plus the board's own draw
LED_MA = (2, 3, 5)           # green LED, lit while a stone is timed
PANEL_EXTRA_MA = 5           # e-ink panel while it updates
BEEP_MA, BEEP_S = 25, 0.06   # passive buzzer per beep
BATTERY_MAH = 390


def simulate():
    period = GAME_S / (ENDS * STONES_PER_END)
    awake = []                 # (start, end) intervals
    partial, full = 2, 0       # switch-on: the menu and the mode, fast whole-screen updates
    awake.append((0.0, 2 * PARTIAL_S + 1.0))
    led_s = beeps = wakes = 0
    stones_since_clean = STONES_PER_CLEAN - 1  # the first ghost-clean follows the first stone
    for i in range(ENDS * STONES_PER_END):
        t0 = i * period
        # timing: start, B2H, H2H; awake until the H2H update has drawn
        partial += 2
        beeps += 3
        wakes += 3
        awake.append((t0, t0 + CLICK_AWAKE_S))                                  # start
        awake.append((t0 + B2H_S, t0 + B2H_S + CLICK_AWAKE_S + PARTIAL_S))      # B2H
        t_h2h = t0 + B2H_S + H2H_S
        awake.append((t_h2h, t_h2h + CLICK_AWAKE_S + PARTIAL_S))                # H2H
        led_s += B2H_S + H2H_S
        # next tip, or ghost clearing every 4th stone
        stones_since_clean += 1
        t_ref = t0 + B2H_S + H2H_S + REFRESH_DELAY_S
        if stones_since_clean >= STONES_PER_CLEAN:
            stones_since_clean = 0
            full += 1
            awake.append((t_ref, t_ref + FULL_S + WAKE_OVERHEAD_S))
        else:
            partial += 1
            awake.append((t_ref, t_ref + PARTIAL_S + WAKE_OVERHEAD_S))
        wakes += 1
        # your own rocks: log the shot
        if i % STONES_PER_END in OWN_ROCKS:
            t_shot = t0 + SHOT_LOGGED_AFTER_S
            partial += 1
            beeps += 1
            wakes += 1
            awake.append((t_shot, t_shot + SHOT_AWAKE_S))

    for k in range(int(GAME_S // AUDIT_EVERY_S)):
        awake.append((k * AUDIT_EVERY_S, k * AUDIT_EVERY_S + AUDIT_AWAKE_S))
        wakes += 1

    # merge overlapping awake intervals
    awake.sort()
    merged = []
    for a, b in awake:
        if merged and a <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], b))
        else:
            merged.append((a, b))
    awake_s = sum(b - a for a, b in merged)
    return partial, full, awake_s, led_s, beeps, wakes, period


def main():
    partial, full, awake_s, led_s, beeps, wakes, period = simulate()
    sleep_s = GAME_S - awake_s
    panel_s = partial * PARTIAL_S + full * FULL_S
    print(f"Game: {ENDS} ends x {STONES_PER_END} stones in {GAME_S / 3600:.0f} h, one stone every {period:.1f} s")
    print(f"Partial updates: {partial}   full refreshes: {full} (plus 1 at switch-off)")
    print(f"Awake: {awake_s:.0f} s = {100 * awake_s / GAME_S:.1f}% of the game; {wakes} wake-ups")
    print(f"Screen busy: {panel_s:.0f} s; LED on: {led_s:.0f} s; beeps: {beeps}")
    for label, k in (("low", 0), ("typical", 1), ("high", 2)):
        mah = (awake_s * AWAKE_MA[k] + sleep_s * SLEEP_MA[k] + led_s * LED_MA[k]
               + panel_s * PANEL_EXTRA_MA + beeps * BEEP_S * BEEP_MA) / 3600
        print(f"Battery, {label:7s}: {mah:5.1f} mAh = {100 * mah / BATTERY_MAH:4.1f}% of {BATTERY_MAH} mAh")


if __name__ == "__main__":
    main()
