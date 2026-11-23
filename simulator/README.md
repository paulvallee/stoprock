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
| <kbd>↑</kbd> | rocker **▲** | Made shot (league: next column, or made once you hold <kbd>↓</kbd> for your rocks) |
| <kbd>↓</kbd> | rocker **▼** | Missed shot (league: previous column; hold it for your rocks) |
| <kbd>Enter</kbd> | rocker **●** | Hold 0.5 s to undo the last shot; keep holding to undo another every 0.5 s. Coach mode: press twice quickly to switch page |

The **Restart in** buttons also tell the firmware which rocker is held over the second serial port,
because the emulator's restart may not keep a button pressed through it.

**Start-up modes.** The simulator starts in **competition** mode (switched on with the side button alone). To
pick another, use the **Restart in competition / league / coach** buttons: they switch the device off (side
button held 3 s) and back on with the rocker held, as you would on the device. By keyboard: switch off, then
hold <kbd>↓</kbd> (league) or <kbd>↑</kbd> (coach) and press <kbd>S</kbd>, keeping both held until the screen
changes.

| Mode | Switch on with | What to try |
|---|---|---|
| Competition | <kbd>S</kbd> | Times only, full width, the badge strip at the bottom; a column per teammate; the highlighted player gets the next times and moves on after two rocks; <kbd>↑</kbd> moves it to the next player, <kbd>↓</kbd> to the previous one, <kbd>Enter</kbd> twice quickly changes your own position |
| League | <kbd>↓</kbd> + <kbd>S</kbd> | Times in four columns: your three teammates and THEM (Hog2Hog white on black); the cursor follows the throw order by itself. <kbd>↑</kbd>/<kbd>↓</kbd> next/previous column; hold <kbd>↑</kbd> to move the newest time a column left every 0.33 s (let go where it belongs); hold <kbd>↓</kbd> to log your rocks (<kbd>↑</kbd> made, <kbd>↓</kbd> missed, then back to the columns); <kbd>Enter</kbd> twice quickly changes your position |
| Coach | <kbd>↑</kbd> + <kbd>S</kbd> | 64 rocks for the whole team; <kbd>Enter</kbd> twice quickly switches between the timer page and the rocks page |

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
