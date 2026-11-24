# Game databank

The stoprock keeps every game it records in flash, so a companion app can collect them later. This
document is the contract between the device and that app. The implementation is
[`firmware/lib/stoprock_core/src/databank.cpp`](../firmware/lib/stoprock_core/src/databank.cpp), tested in
[`firmware/test/test_databank`](../firmware/test/test_databank/test_main.cpp).

## What is kept

One record per game:

| Field | Meaning |
|---|---|
| `id` | 1, 2, 3, … Never reused, even after games are dropped or erased, so an app can tell new games from old ones by number alone |
| `started`, `updated` | Clock seconds (Unix time, UTC) when the game started and when it last changed |
| `clock` | `false` if the device's clock hadn't been set; then `started` and `updated` only make sense relative to each other |
| `mode` | How the device was switched on: `competition`, `league`, `coach` or `zoom` (no players: every stone's player is `null`; games before format version 3 read as `league`) |
| `shots` | Rocks in order: `M` made, `X` missed. League: your own, up to 16. Coach: the team's, up to 64, in throwing order (lead, lead, second, second, third, third, skip, skip per end). Competition: none |
| `made`, `thrown`, `pct` | Summary of `shots`; `pct` is -1 with no rocks thrown |
| `stones` | Every timed stone in order, as `[back-to-hog, hog-to-hog, player, start, guess]`: times in seconds, `null` where that split wasn't timed; player is who threw it, `"lead"`, `"second"`, `"third"` or `"skip"`, or `"them"` for the other team (`null` in games before format version 4, and in coach games before format version 5); start is when the stone started (its first click), in whole seconds since switch-on; guess is who the device's throw order guessed when it started, whatever the cursor showed, so a guess that differs from player is one you corrected (start and guess `null` before format version 5). Up to 128. For an H2H-only stone, start is the click at the near hog line, about 3 s after the stone's real start |
| `shot_times` | When each rock in `shots` was logged, in whole seconds since switch-on (`null` before format version 5). League: about 20–30 s after you threw, once your rock stopped |
| `battery_start`, `battery_end` | Battery charge (%) at switch-on and at switch-off; `null` if unknown (no reading, a game recorded before format version 2, or a game that never switched off cleanly) |

Rules:

- **Every switch-on starts a new game.** Light sleep between stones doesn't restart the chip, so a game is
  never split while you're playing.
- A game is written only once it has a shot or a time in it.
- It is rewritten on every change, so a game is complete up to the last press even if the battery dies.
- Up to **500 games**. When a 501st is stored, the oldest is dropped, synced or not. If the flash fills up
  first (a full game is up to about 1.2 KB), the oldest games are dropped to make room for the one being played.

## Storage

The databank has its own NVS partition, `games` (256 KB at 0x310000, see
[`firmware/partitions.csv`](../firmware/partitions.csv)), separate from settings. NVS spreads writes across
the flash and commits each record atomically.

Keys in the namespace `bank`:

- `meta`: version (1), oldest id, next id, last acknowledged id, as little-endian u32s.
- `g<id>`: one game, record format version 5:

| Bytes | Field |
|---|---|
| 0 | `'G'` |
| 1 | Format version (5) |
| 2 | Flags: bit 0 = clock set |
| 3 | Rocks thrown (0–64) |
| 4–7 | id |
| 8–11 | Started |
| 12–15 | Updated |
| 16–17 | Number of timed stones *n* (0–128) |
| 18 | Battery % at switch-on (0xFF = unknown) |
| 19 | Battery % at switch-off (0xFF = unknown) |
| 20 | Mode: 0 league, 1 competition, 2 coach |
| 21–23 | Reserved (0) |
| 24–31 | Shot bits (u64): bit *i* set = rock *i* made |
| 32 … 32 + 8*n* − 1 | Per stone: back-to-hog, hog-to-hog (u16 hundredths; 0xFFFF = not timed), player (u8: 0 lead, 1 second, 2 third, 3 skip, 4 the other team; 0xFF = none), guess (u8, the same codes: who the throw order guessed when the stone started), start (u16 seconds since switch-on; 0xFFFF = unknown) |
| then 2 per rock thrown | When each rock was logged (u16 seconds since switch-on; 0xFFFF = unknown) |
| last | Checksum of all preceding bytes |

All numbers are little-endian. A record that fails its checks is skipped, not shown as a game.

Older records are still read:

- Version 4: the same with 5 bytes per stone (no guess or start) and no shot times.
- Version 3: the same with 4 bytes per stone (no player).

Versions 1 and 2 read as league games:

- Version 2: a 24-byte header. Bytes 0–15 as above (rocks thrown 0–16), 16–17 shot bits, 18–19 stone count,
  20–21 battery at switch-on and switch-off, 22–23 reserved; stones from byte 24.
- Version 1: the same as version 2 with bytes 20–23 reserved: the battery reads as unknown.

## Checking the throw order against real games

`python tools/analyze_games.py games.txt` reads exported games (the `@GAMES` listing, or JSON lines) and
reports, for every game with start times: each stone's start, the gap since the previous one, the device's
guess and where it ended up; how often the guess was right, by gap; and the gaps between stones a throw apart
(the other team's stone after ours, or ours after theirs), which is what the 80 s and ~56 s-a-throw rules
assume. The player you left a stone under is taken as the truth, so move wrong times before exporting.

## Battery drain log

Each game records the battery at switch-on and switch-off. At switch-off the device estimates the games left
for the help page: the battery now, divided by the average drain of the last 10 completed games, rounded down, less one game
kept in reserve (so "Charge before next game" shows while about one game's charge is still left).
A game counts only if it has both readings, lost at least 1 %, and lasted at least 20 minutes, so a quick
on-and-off or a game played on USB power doesn't skew it. The readings are M5Stack's voltage-based estimate,
so expect the figure to move around by a game or two.

## Talking to the device

Over the USB serial console (115200 baud) for now; the Bluetooth companion app will carry the same commands
and replies. One command per line.

| Command | Reply |
|---|---|
| `bank` | `@BANK {"first":1,"last":42,"synced":40,"current":42,"version":5}` |
| `games` | Every stored game |
| `games new` | Games after the last acknowledged one |
| `game <id>` | Games from that id on |
| `ack <id>` | Marks every game up to `id` as collected: `@ACK <id>` |
| `erase-all yes` | Deletes every stored game (ids keep counting): `@ERASED` |
| `crash` | What crashes left behind (below), and how much stack the two tasks have spare |
| `crash clear` | Forgets them |
| `t <unix seconds>` | Sets the clock (until the app does it) |

Game listings are framed so they can be parsed from a stream that also carries log lines. Each starts
with a `@CRASH` line (see Crashes below), then the games:

```text
@CRASH {"crashes":0,"resets":0,"last_reset":"","last":""}
@GAMES 2
{"id":41,"v":5,"mode":"league","started":1795290802,"updated":1795297990,"clock":true,"shots":"MX","made":1,"thrown":2,"pct":50,"stones":[[3.47,13.92,"lead",12,"lead"],[3.29,null,"them",66,"them"],[null,14.02,"third",290,"them"]],"shot_times":[148,262],"battery_start":88,"battery_end":85}
{"id":42,"v":5,"mode":"coach","started":1795377202,"updated":1795377300,"clock":true,"shots":"M","made":1,"thrown":1,"pct":100,"stones":[],"shot_times":[41],"battery_start":85,"battery_end":null}
@END
```

`current` in `@BANK` is the id of the game in progress, or 0 if it hasn't been stored yet. Its record keeps
changing until the device is switched off.

## Crashes

On battery a crash looks like the device switching itself off: the restart lets go of the power latch, so
the next start is a plain power-on. The panic handler first writes a core dump to the `coredump` partition
(128 KB, see `partitions.csv`); the next start reads its summary, keeps it with the settings and erases
the dump. `@CRASH` reports:

- `crashes`: core dumps found since the last `crash clear`; `last`: the newest one's summary: the clock when
  it was found, the task, the program counter, the exception cause and address, the first 8 characters of
  the build's ELF hash, and the backtrace. Decode the addresses with the matching build's ELF:
  `xtensa-esp32-elf-addr2line -pfiaC -e firmware/.pio/build/coreink/firmware.elf <addresses>`.
- `resets`: starts after a crash, watchdog or brownout that did restart (on USB power); `last_reset`: the
  newest one's kind.

## How an app should sync

1. Set the clock: `t <now>`.
2. `games new`, and store each game by `id`. Replace any game you already have with the same id: the game in
   progress can change.
3. `ack <highest id you stored>`. Don't acknowledge the game in progress (`current` in `@BANK`) until a
   later sync shows it finished, or you'll miss its last changes.

Acknowledging deletes nothing on the device. It only moves the bookmark that `games new` starts from.
