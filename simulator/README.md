# stoprock simulator

Runs the real stoprock firmware in Espressif's free, open-source
[esp-emulator](https://github.com/espressif/esp-emulator) (Apache 2.0) and shows the device in your browser:
the screen, the buttons, the green LED and the beeps.

esp-emulator doesn't emulate the original ESP32 in the CoreInk, so the simulator build targets an
**ESP32-S3**, which it does emulate. The application code is the same; only the board layer differs
([`firmware/src/board_emu.cpp`](../firmware/src/board_emu.cpp)). The emulated firmware sends its screen
updates, beeps and LED over the serial console, and `run.py` turns them into the web page. Screen updates take as
long as on the CoreInk's e-ink: the firmware's display task is busy for 0.26 s per partial update and 2 s per
full refresh, and the page shows each new image only when the panel would have finished it, with a full
refresh flashing for its whole 2 s. The delays run on the emulated chip's clock, so if the emulator runs
slower than real time on your computer, everything on the page slows down with it.

## Setup (macOS Apple Silicon or Linux, once)

1. Install esp-emulator (it goes to `~/.local/bin/esp-emu`):

   ```sh
   curl -fsSL https://raw.githubusercontent.com/espressif/esp-emulator/main/install.sh | sh
   ```

2. Install PlatformIO, to build the firmware. On a Mac with Homebrew:

   ```sh
   brew install platformio
   ```

   Elsewhere, `pipx install platformio` works. Plain `pip install` is refused by Homebrew's Python.

## Run

From the repository's top folder:

```sh
cd firmware
pio run -e emu
cd ..
python3 simulator/run.py
```

The first build downloads the ESP32 tools, which takes a few minutes. `run.py` then opens
<http://localhost:8200> in your browser (open it yourself if it doesn't appear). The
first run also downloads the push-button part (`pushbutton.wasm`) from the esp-emulator examples kit into
`simulator/parts/`.

## Controls

| Key | Mouse | Button |
|---|---|---|
| <kbd>Space</kbd> or <kbd>T</kbd> | **top** | Timing click |
| <kbd>S</kbd> | **side / power** | Timing click; hold 3 s to switch off (shows the help page; press it again to switch back on) |
| <kbd>↑</kbd> | rocker **▲** | Next column; on the rocks (league, coach: hold <kbd>↓</kbd> to get there), made. Hold to move the newest time (on the rocks: back to the times) |
| <kbd>↓</kbd> | rocker **▼** | Previous column, or missed on the rocks; hold it for the rocks (league, coach) or to move the newest time left (competition) |
| <kbd>Enter</kbd> | rocker **●** | Hold 1 s to undo the newest time or shot ("Keep holding / to undo" shows after 0.2 s); keep holding to undo another every 1 s. Press twice quickly: change your position (coach: switch page) |

**Export games** asks the device for every game it has recorded and saves the listing to
`simulator/exports/games-<date>-<time>.txt` (the page shows the path). Analyse it with
`python3 tools/analyze_games.py simulator/exports/<file>`, or attach the file to share it.

The **Restart in** buttons also tell the firmware which rocker is held over the second serial port,
because the emulator's restart may not keep a button pressed through it.

**Start-up modes.** The simulator starts **switched off**, showing the off page, as a device would: press <kbd>S</kbd> to
switch it on, and the **mode menu** comes up: <kbd>↑</kbd>/<kbd>↓</kbd> choose, <kbd>Enter</kbd> (the rocker's middle)
starts the mode selected (the one used last, at first). To skip the menu, hold the rocker while switching on: switch off,
then hold <kbd>Enter</kbd> (competition), <kbd>↓</kbd> (league) or <kbd>↑</kbd> (coach) and press <kbd>S</kbd>, keeping
both held until the screen changes. The **Restart in competition / league / coach** buttons do that for you: they switch
the device off (side button held 3 s) and back on with the rocker held.

| Mode | Switch on with | What to try |
|---|---|---|
| Competition | <kbd>S</kbd> | Times only, the badge strip at the bottom; four columns as in league (your teammates and THEM); the cursor follows the throw order by itself. <kbd>↑</kbd>/<kbd>↓</kbd> next/previous column; hold <kbd>↑</kbd> to move the newest time a column right every 0.7 s; <kbd>Enter</kbd> twice quickly changes your own position |
| League | <kbd>↓</kbd> + <kbd>S</kbd> | Times in four columns: your three teammates and THEM (Hog2Hog white on black); the cursor follows the throw order by itself. <kbd>↑</kbd>/<kbd>↓</kbd> next/previous column; hold <kbd>↑</kbd> to move the newest time a column right every 0.7 s (let go where it belongs); hold <kbd>↓</kbd> to log your rocks (<kbd>↑</kbd> made, <kbd>↓</kbd> missed, then back to the columns); <kbd>Enter</kbd> twice quickly changes your position |
| Coach | <kbd>↑</kbd> + <kbd>S</kbd> | 64 rocks for the whole team; page 1 has times in five columns (Lead, 2nd, 3rd, Skip, THEM) with the cursor following the throw order. As in league: <kbd>↑</kbd>/<kbd>↓</kbd> next/previous column, hold <kbd>↑</kbd> to move the newest time, hold <kbd>↓</kbd> for the rocks (<kbd>↑</kbd> made, <kbd>↓</kbd> missed; hold <kbd>↑</kbd> back to the times); <kbd>Enter</kbd> twice quickly switches between the timer page and the rocks page |
| Basic | <kbd>S</kbd>, then choose it in the menu (the first entry) | Times only, as big as they go: a row a stone, 4 rows, B2H left, H2H right, no players; <kbd>↓</kbd>/<kbd>↑</kbd> scroll to older/newer stones (the badge says which show); hold <kbd>Enter</kbd> to undo |

The **Battery** slider sets the battery level the firmware reads (`run.py` sends it to the emulated chip's
second serial port). Below 15% the screen shows the low-battery warning (league: a bar over your %; competition: the badge strip), within about 2 s. To try the battery
drain log and the "~N games battery left" estimate on the off screen: set a level, switch on, log a shot or a
time, wait at least a minute, lower the slider, then hold the side button 3 s to switch off. In the simulator a
game of 1 minute or more counts towards the estimate (on the CoreInk: 20 minutes), so a few short games are enough.

## What it doesn't show

- **Light sleep and wake-up.** esp-emulator doesn't model light sleep on the S3, so the simulator stays awake.
- **E-ink ghosting and the real panel's look.** The page draws clean pixels; a full refresh flashes black and
  white to show when one happens.
- **CoreInk specifics:** real pins, battery level, the clock chip and the power switch.

## Files

| File | What |
|---|---|
| `run.py` | Starts esp-emu with the board, serves the page, relays button presses over esp-emu's control channel and the battery slider over UART1 |
| `viewer.html` | The page: device, screen, keys, sound |
| `board.json` | esp-emulator board: five push-button parts on GPIO 4, 5, 6, 7 and 15 |
| `parts/` | Downloaded parts (not committed) |
