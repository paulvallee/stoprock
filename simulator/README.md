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
| <kbd>↑</kbd> | rocker **▲** | Made shot |
| <kbd>↓</kbd> | rocker **▼** | Missed shot |
| <kbd>Enter</kbd> | rocker **●** | Hold 0.5 s to undo the last shot; keep holding to undo another every 0.5 s |

The **Battery** slider sets the battery level the firmware reads (`run.py` sends it to the emulated chip's
second serial port). Below 15% the tip strip shows the low-battery warning, within about 2 s. To try the battery
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
