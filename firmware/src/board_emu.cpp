// Simulator board for Espressif's esp-emulator (ESP32-S3). The emulator has no e-ink
// screen, so this board sends the screen, beeps and LED to the console as lines the
// viewer in simulator/ draws and plays:
//
//   @FLASH ms                a full refresh has started; it flashes for ms milliseconds
//   @FB x y w h full HEX     a screen region, sent when the panel would have finished drawing it
//                            (x, w multiples of 8; HEX = rows of w/8 bytes, 1 = black)
//   @TONE hz                 buzzer frequency, 0 = silent
//   @LED 0|1                 green LED
//
// Buttons are esp-emulator push-button parts on the pins below (simulator/board.json).
// The battery level comes from the viewer's slider: run.py sends "battery N" lines to UART1,
// which esp-emulator bridges to a TCP port (--uart1-tcp). Before a "restart in <mode>" the viewer
// also sends "hold up|down|none": the rocker held for the next start-up, in case the emulator's
// restart doesn't keep the push-buttons' state.
#include <Arduino.h>
#include <esp_attr.h>

#include "board.h"

namespace board {
namespace {

constexpr int kPins[kButtonCount] = {
    4,   // top button
    5,   // side power button
    6,   // rocker up
    7,   // rocker press
    15,  // rocker down
};
constexpr int kUart1Rx = 17, kUart1Tx = 18;
constexpr uint32_t kBatteryMagic = 0xBA77E21;
constexpr int64_t kPartialUs = 260000;   // GDEY0154D67 partial refresh
constexpr int64_t kFullUs = 2000000;     // full refresh

uint32_t clockBase = 0;   // seconds at clockBaseMs
uint32_t clockBaseMs = 0;
bool clockIsSet = false;

// Battery level from the slider. Kept in RTC memory so a restart (switch-off and on in the
// simulator) still knows it before the next "battery" line arrives.
RTC_NOINIT_ATTR int savedBattery;
RTC_NOINIT_ATTR uint32_t savedBatteryMagic;
int battery = -1;
// The rocker the viewer says is held for the next start-up (kButtonCount = none), until used.
constexpr uint32_t kHoldMagic = 0x401DB07;
RTC_NOINIT_ATTR uint32_t savedHold;
RTC_NOINIT_ATTR uint32_t savedHoldMagic;
int holdAtStart = kButtonCount;

// Read any lines waiting on UART1 ("battery N", "hold up|down|none"); true if a battery one arrived.
bool readBatteryLines() {
    static char line[24];
    static uint8_t len = 0;
    bool got = false;
    while (Serial1.available()) {
        const char c = static_cast<char>(Serial1.read());
        if (c != '\n' && c != '\r') { if (len < sizeof line - 1) line[len++] = c; continue; }
        line[len] = 0;
        len = 0;
        if (!strncmp(line, "battery ", 8)) {
            battery = constrain(atoi(line + 8), 0, 100);
            savedBattery = battery;
            savedBatteryMagic = kBatteryMagic;
            got = true;
        } else if (!strncmp(line, "hold ", 5)) {
            const char* w = line + 5;
            savedHold = !strcmp(w, "up") ? kUp : !strcmp(w, "down") ? kDown : kButtonCount;
            savedHoldMagic = kHoldMagic;
        }
    }
    return got;
}

}  // namespace

void begin() {
    for (int p : kPins) pinMode(p, INPUT_PULLUP);
    Serial1.begin(115200, SERIAL_8N1, kUart1Rx, kUart1Tx);
    if (savedHoldMagic == kHoldMagic && savedHold < kButtonCount) holdAtStart = static_cast<int>(savedHold);
    savedHoldMagic = 0;  // used once
    if (savedBatteryMagic == kBatteryMagic) {
        battery = savedBattery;
    } else {  // first start: wait briefly for the viewer's slider value
        const uint32_t start = millis();
        while (!readBatteryLines() && millis() - start < 1500) delay(10);
    }
}

bool held(Button b) { return digitalRead(kPins[b]) == LOW; }
bool heldAtStart(Button b) { return held(b) || holdAtStart == b; }
int pin(Button b) { return kPins[b]; }

void show(const stoprock::Canvas& c, stoprock::Rect r, bool full) {
    // Behave like the e-ink: a full refresh flashes for its whole duration, and the new
    // image only becomes visible when the panel has finished (0.26 s or 2 s from now).
    const int64_t t0 = esp_timer_get_time();
    if (full) Serial.printf("@FLASH %d\n", static_cast<int>(kFullUs / 1000));
    const int64_t wait = (full ? kFullUs : kPartialUs) - (esp_timer_get_time() - t0);
    if (wait > 0) delay(wait / 1000);

    static const char hex[] = "0123456789ABCDEF";
    Serial.printf("@FB %d %d %d %d %d ", r.x, r.y, r.w, r.h, full ? 1 : 0);
    char line[2 * stoprock::Canvas::STRIDE + 1];
    for (int y = r.y; y < r.y + r.h; ++y) {
        const uint8_t* row = c.data() + y * stoprock::Canvas::STRIDE + r.x / 8;
        int n = 0;
        for (int i = 0; i < r.w / 8; ++i) {
            line[n++] = hex[row[i] >> 4];
            line[n++] = hex[row[i] & 15];
        }
        line[n] = 0;
        Serial.print(line);
    }
    Serial.println();
}

void tone(uint16_t hz) { Serial.printf("@TONE %u\n", hz); }
void led(bool on) { Serial.printf("@LED %d\n", on ? 1 : 0); }

int batteryPercent() {
    readBatteryLines();
    return battery;  // -1 until the viewer has sent a value
}

uint32_t clockNow() { return clockBase + (millis() - clockBaseMs) / 1000; }
bool clockValid() { return clockIsSet; }
void clockSet(uint32_t unixSeconds) {
    clockBase = unixSeconds;
    clockBaseMs = millis();
    clockIsSet = true;
}

bool lightSleep(int64_t) { return false; }
bool rtcSeconds(uint32_t*) { return false; }  // no real-time clock chip  // the emulator doesn't model light sleep on the S3

void powerOff() {
    // Nothing to switch off: wait for the side button to be let go (it was held to switch
    // off), then restart as soon as it's pressed again, as the CoreInk switches on while
    // the button is held. The app ignores a button already held when it starts.
    while (digitalRead(kPins[kSide]) == LOW) delay(20);
    while (digitalRead(kPins[kSide]) == HIGH) { readBatteryLines(); delay(20); }  // also takes "hold ..."
}

const char* name() { return "esp-emulator"; }

}  // namespace board
