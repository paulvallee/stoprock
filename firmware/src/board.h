// Hardware layer. One implementation per target: board_coreink.cpp (M5Stack CoreInk)
// and board_emu.cpp (Espressif esp-emulator: ESP32-S3, screen and sounds sent to simulator/).
#pragma once
#include <stdint.h>

#include "canvas.h"

namespace board {

enum Button : uint8_t { kTop, kSide, kUp, kPress, kDown, kButtonCount };

void begin();

// True while the button is held (raw level, the app debounces).
bool held(Button b);
// Held when the firmware started: what picks the mode. On the CoreInk the same as held(); the
// simulator also accepts it from the viewer, as a restart there may not keep a button's state.
bool heldAtStart(Button b);
// GPIO number of a button, for interrupts and wake-up.
int pin(Button b);

// Push the region r of c to the screen and wait until it is visible.
// full = ghost-clearing refresh of the whole screen.
void show(const stoprock::Canvas& c, stoprock::Rect r, bool full);

void tone(uint16_t hz);  // 0 = silent
void led(bool on);

// Battery charge 0-100, or -1 if unknown.
int batteryPercent();

// Real-time clock, seconds since 1970 (UTC). valid() is false until the clock has been set.
uint32_t clockNow();
bool clockValid();
void clockSet(uint32_t unixSeconds);

// The real-time clock's current second (one quick I2C read). False if this board has no
// real-time clock chip. The clock audit watches for it to tick over.
bool rtcSeconds(uint32_t* seconds);

// Light sleep until a button is pressed or wakeAtUs (esp_timer time; -1 = none) passes.
// Returns false if this board doesn't sleep (the app then just idles).
bool lightSleep(int64_t wakeAtUs);

// A console command from somewhere other than the USB serial port (the simulator's page sends them
// on its second port, e.g. "games" for its Export button). False if none is waiting.
bool command(char* out, size_t len);

// Switch off (the e-ink keeps the last image). Returns only where power-off is impossible
// (on USB power, or in the simulator), after the user presses the side button again.
void powerOff();
// True when this start should be "off": the simulator's first start, which shows the off page and
// waits for the side button as a device would. Never on the CoreInk (it only runs once switched on).
bool startsOff();

const char* name();

}  // namespace board
