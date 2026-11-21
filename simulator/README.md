# stoprock simulator

Runs the real stoprock firmware in Espressif's free, open-source
[esp-emulator](https://github.com/espressif/esp-emulator) (Apache 2.0) and shows the device in your browser:
the screen, the buttons, the green LED and the beeps.

esp-emulator doesn't emulate the original ESP32 in the CoreInk, so the simulator build targets an
**ESP32-S3**, which it does emulate. The application code is the same; only the board layer differs
([`firmware/src/board_emu.cpp`](../firmware/src/board_emu.cpp)). The emulated firmware sends its screen
updates, beeps and LED over the serial console, and `run.py` turns them into the web page. Screen updates are
delayed to match the CoreInk's e-ink: 0.26 s, or 2 s for a full refresh.

## Setup (macOS Apple Silicon or Linux, once)

```sh
# 1. esp-emulator (installs to ~/.local/bin/esp-emu)
curl -fsSL https://raw.githubusercontent.com/espressif/esp-emulator/main/install.sh | sh

# 2. PlatformIO, to build the firmware
python3 -m pip install --user platformio
```

## Run

```sh
cd firmware
python3 -m platformio run -e emu   # first build downloads the ESP32 tools (a few minutes)
cd ..
python3 simulator/run.py
```

Then open <http://localhost:8200>. The first run downloads the push-button part (`pushbutton.wasm`) from the
esp-emulator examples kit into `simulator/parts/`.

## Controls

| Key | Mouse | Button |
|---|---|---|
| <kbd>Space</kbd> or <kbd>T</kbd> | **top** | Timing click |
| <kbd>S</kbd> | **side / power** | Timing click; hold 3 s to switch off (shows the help page; press it again to switch back on) |
| <kbd>↑</kbd> | rocker **▲** | Made shot |
| <kbd>↓</kbd> | rocker **▼** | Missed shot |
| <kbd>Enter</kbd> | rocker **●** | Hold 1 s to undo the last shot |

The **Low battery** box holds down a sixth virtual button that the simulator board reads as a low battery.

## What it doesn't show

- **Light sleep and wake-up.** esp-emulator doesn't model light sleep on the S3, so the simulator stays awake.
- **E-ink ghosting and the real panel's look.** The page draws clean pixels; a full refresh flashes black and
  white to show when one happens.
- **CoreInk specifics:** real pins, battery level, the clock chip and the power switch.

## Files

| File | What |
|---|---|
| `run.py` | Starts esp-emu with the board, serves the page, relays button presses over esp-emu's control channel |
| `viewer.html` | The page: device, screen, keys, sound |
| `board.json` | esp-emulator board: six push-button parts on GPIO 4, 5, 6, 7, 15 and 16 |
| `parts/` | Downloaded parts (not committed) |
