"""Model a game against the firmware's rules (firmware/src/app.cpp) to count screen
refreshes, awake time and battery use.

    python3 tools/game_sim.py

Rules modelled (docs/SPEC.md sections 4, 7, 10):
- A stone's first click wakes the chip; it stays fully awake until the stone is done.
- B2H result: one partial update. H2H result: one partial update, then sleep at once.
- 5 s after each H2H the chip wakes for the next tip: a partial update, or a full
  (ghost-clearing) refresh after every 4th stone. Then it sleeps again.
- A rocker press wakes it, logs the shot (one partial update when no stone is being
  timed) and keeps it awake 20 s.
- Switching on is a full refresh. Switching off (help page) is one more, after the game.
"""

GAME_S = 2 * 3600
ENDS, STONES_PER_END = 8, 16
B2H_S, H2H_S = 4.0, 15.0
PARTIAL_S, FULL_S = 0.26, 2.0
STONES_PER_CLEAN = 4
SHOT_AWAKE_S = 20.0
REFRESH_DELAY_S = 5.0
OWN_ROCKS = (0, 2)           # your two rocks in each end (as lead: stones 1 and 3)
SHOT_LOGGED_AFTER_S = 35.0   # you log your rock ~35 s after its back-line click, once it stops
WAKE_OVERHEAD_S = 0.02       # wake-up, run the loop, go back to sleep

# Estimated currents at 3.7 V (to be measured on a real CoreInk), low / typical / high
AWAKE_MA = (25, 35, 50)      # ESP32 at 240 MHz, radio off, mostly waiting
SLEEP_MA = (1.0, 2.0, 3.0)   # ESP32 light sleep plus the board's own draw
LED_MA = (2, 3, 5)           # green LED, lit while a stone is timed
PANEL_EXTRA_MA = 5           # e-ink panel while it updates
BEEP_MA, BEEP_S = 25, 0.06   # passive buzzer per beep
BATTERY_MAH = 390


def simulate():
    period = GAME_S / (ENDS * STONES_PER_END)
    awake = []                 # (start, end) intervals
    partial, full = 0, 1       # switch-on is a full refresh
    awake.append((0.0, FULL_S + 1.0))
    led_s = beeps = wakes = 0
    stones_since_clean = 0
    for i in range(ENDS * STONES_PER_END):
        t0 = i * period
        # timing: start, B2H, H2H; awake until the H2H update has drawn
        partial += 2
        beeps += 3
        wakes += 1
        awake.append((t0, t0 + B2H_S + H2H_S + PARTIAL_S))
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
