// Simulator board for Espressif's esp-emulator (ESP32-S3). The emulator has no e-ink
// screen, so this board sends the screen, beeps and LED to the console as lines the
// viewer in simulator/ draws and plays:
//
//   @FB x y w h full HEX     a screen region (x, w multiples of 8; HEX = rows of w/8 bytes, 1 = black)
//   @TONE hz                 buzzer frequency, 0 = silent
//   @LED 0|1                 green LED
//
// Buttons are esp-emulator push-button parts on the pins below (simulator/board.json).
// A sixth button part on GPIO 16 stands in for a low battery while held.
#include <Arduino.h>

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
constexpr int kLowBatteryPin = 16;
constexpr int64_t kPartialUs = 260000;   // GDEY0154D67 partial refresh
constexpr int64_t kFullUs = 2000000;     // full refresh

uint32_t clockBase = 0;   // seconds at clockBaseMs
uint32_t clockBaseMs = 0;
bool clockIsSet = false;

}  // namespace

void begin() {
    for (int p : kPins) pinMode(p, INPUT_PULLUP);
    pinMode(kLowBatteryPin, INPUT_PULLUP);
}

bool held(Button b) { return digitalRead(kPins[b]) == LOW; }
int pin(Button b) { return kPins[b]; }

void show(const stoprock::Canvas& c, stoprock::Rect r, bool full) {
    const int64_t t0 = esp_timer_get_time();
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
    // Take as long as the real screen would.
    const int64_t wait = (full ? kFullUs : kPartialUs) - (esp_timer_get_time() - t0);
    if (wait > 0) delay(wait / 1000);
}

void tone(uint16_t hz) { Serial.printf("@TONE %u\n", hz); }
void led(bool on) { Serial.printf("@LED %d\n", on ? 1 : 0); }

int batteryPercent() { return digitalRead(kLowBatteryPin) == LOW ? 10 : 80; }

uint32_t clockNow() { return clockBase + (millis() - clockBaseMs) / 1000; }
bool clockValid() { return clockIsSet; }
void clockSet(uint32_t unixSeconds) {
    clockBase = unixSeconds;
    clockBaseMs = millis();
    clockIsSet = true;
}

bool lightSleep(int64_t) { return false; }  // the emulator doesn't model light sleep on the S3

void powerOff() {
    // Nothing to switch off: wait for the side button to be let go, pressed and let go
    // again (standing in for "hold side button to start"), then the app restarts.
    while (digitalRead(kPins[kSide]) == LOW) delay(20);
    while (digitalRead(kPins[kSide]) == HIGH) delay(20);
    while (digitalRead(kPins[kSide]) == LOW) delay(20);
}

const char* name() { return "esp-emulator"; }

}  // namespace board
