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
| `mode` | How the device was switched on: `competition`, `league` or `coach` (games before format version 3 read as `league`) |
| `shots` | Rocks in order: `M` made, `X` missed. League: your own, up to 16. Coach: the team's, up to 64, in throwing order (lead, lead, second, second, third, third, skip, skip per end). Competition: none |
| `made`, `thrown`, `pct` | Summary of `shots`; `pct` is -1 with no rocks thrown |
| `stones` | Every timed stone in order, as `[back-to-hog, hog-to-hog, player]`: times in seconds, `null` where that split wasn't timed; player is who threw it in a competition or league game, `"lead"`, `"second"`, `"third"` or `"skip"` (`null` in coach games, and in games before format version 4). Up to 128 |
| `battery_start`, `battery_end` | Battery charge (%) at switch-on and at switch-off; `null` if unknown (no reading, a game recorded before format version 2, or a game that never switched off cleanly) |

Rules:

- **Every switch-on starts a new game.** Light sleep between stones doesn't restart the chip, so a game is
  never split while you're playing.
- A game is written only once it has a shot or a time in it.
- It is rewritten on every change, so a game is complete up to the last press even if the battery dies.
- Up to **500 games**. When a 501st is stored, the oldest is dropped, synced or not.

## Storage

The databank has its own NVS partition, `games` (256 KB at 0x310000, see
[`firmware/partitions.csv`](../firmware/partitions.csv)), separate from settings. NVS spreads writes across
the flash and commits each record atomically.

Keys in the namespace `bank`:

- `meta`: version (1), oldest id, next id, last acknowledged id, as little-endian u32s.
- `g<id>`: one game, record format version 4:

| Bytes | Field |
|---|---|
| 0 | `'G'` |
| 1 | Format version (4) |
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
| 32 … 32 + 5*n* − 1 | Per stone: back-to-hog, hog-to-hog (u16 hundredths; 0xFFFF = not timed), player (u8: 0 lead, 1 second, 2 third, 3 skip; 0xFF = none) |
| last | Checksum of all preceding bytes |

All numbers are little-endian. A record that fails its checks is skipped, not shown as a game.

Older records are still read:

- Version 3: the same with 4 bytes per stone (no player).

Versions 1 and 2 read as league games:

- Version 2: a 24-byte header. Bytes 0–15 as above (rocks thrown 0–16), 16–17 shot bits, 18–19 stone count,
  20–21 battery at switch-on and switch-off, 22–23 reserved; stones from byte 24.
- Version 1: the same as version 2 with bytes 20–23 reserved: the battery reads as unknown.

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
| `bank` | `@BANK {"first":1,"last":42,"synced":40,"current":42,"version":4}` |
| `games` | Every stored game |
| `games new` | Games after the last acknowledged one |
| `game <id>` | Games from that id on |
| `ack <id>` | Marks every game up to `id` as collected: `@ACK <id>` |
| `erase-all yes` | Deletes every stored game (ids keep counting): `@ERASED` |
| `t <unix seconds>` | Sets the clock (until the app does it) |

Game listings are framed so they can be parsed from a stream that also carries log lines:

```text
@GAMES 2
{"id":41,"v":4,"mode":"league","started":1795290802,"updated":1795297990,"clock":true,"shots":"MMXMMMXM","made":6,"thrown":8,"pct":75,"stones":[[3.47,13.92,null],[3.29,null,null],[null,14.02,null]],"battery_start":88,"battery_end":85}
{"id":42,"v":4,"mode":"coach","started":1795377202,"updated":1795377300,"clock":true,"shots":"M","made":1,"thrown":1,"pct":100,"stones":[],"battery_start":85,"battery_end":null}
@END
```

`current` in `@BANK` is the id of the game in progress, or 0 if it hasn't been stored yet. Its record keeps
changing until the device is switched off.

## How an app should sync

1. Set the clock: `t <now>`.
2. `games new`, and store each game by `id`. Replace any game you already have with the same id: the game in
   progress can change.
3. `ack <highest id you stored>`. Don't acknowledge the game in progress (`current` in `@BANK`) until a
   later sync shows it finished, or you'll miss its last changes.

Acknowledging deletes nothing on the device. It only moves the bookmark that `games new` starts from.
