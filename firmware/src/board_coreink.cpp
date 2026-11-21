// M5Stack CoreInk (current revision, GDEY0154D67 screen). Pins from M5Stack's CoreInk
// library; confirm on the device (docs/SPEC.md open item 1).
#include <Arduino.h>
#include <M5Unified.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <sys/time.h>

#include "board.h"

namespace board {
namespace {

constexpr int kPins[kButtonCount] = {
    5,   // top button
    27,  // side power button
    37,  // rocker up
    38,  // rocker press
    39,  // rocker down
};
constexpr int kLedPin = 10;      // green LED, active low (to confirm)
constexpr int kBuzzerPin = 2;
constexpr int kBuzzerChannel = 0;

uint8_t region[(stoprock::Canvas::W / 8) * stoprock::Canvas::H];

}  // namespace

void begin() {
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    M5.Display.setAutoDisplay(false);
    M5.Display.setRotation(0);

    for (int p : kPins) pinMode(p, INPUT);  // the board has its own pull-ups
    pinMode(kLedPin, OUTPUT);
    digitalWrite(kLedPin, HIGH);
    ledcSetup(kBuzzerChannel, 2000, 8);
    ledcAttachPin(kBuzzerPin, kBuzzerChannel);
    ledcWrite(kBuzzerChannel, 0);
}

bool held(Button b) { return digitalRead(kPins[b]) == LOW; }
int pin(Button b) { return kPins[b]; }

void show(const stoprock::Canvas& c, stoprock::Rect r, bool full) {
    // Copy the region (x and w are multiples of 8) into a packed bitmap.
    const int bytesPerRow = r.w / 8;
    for (int y = 0; y < r.h; ++y)
        memcpy(&region[y * bytesPerRow], c.data() + (r.y + y) * stoprock::Canvas::STRIDE + r.x / 8, bytesPerRow);

    M5.Display.waitDisplay();
    M5.Display.setEpdMode(full ? epd_mode_t::epd_quality : epd_mode_t::epd_fastest);
    M5.Display.startWrite();
    M5.Display.drawBitmap(r.x, r.y, region, r.w, r.h, TFT_BLACK, TFT_WHITE);
    M5.Display.endWrite();
    M5.Display.display(r.x, r.y, r.w, r.h);
    M5.Display.waitDisplay();
}

void tone(uint16_t hz) {
    if (hz == 0) { ledcWrite(kBuzzerChannel, 0); return; }
    ledcWriteTone(kBuzzerChannel, hz);
    ledcWrite(kBuzzerChannel, 128);
}

void led(bool on) { digitalWrite(kLedPin, on ? LOW : HIGH); }

int batteryPercent() { return M5.Power.getBatteryLevel(); }

uint32_t clockNow() {
    if (!M5.Rtc.isEnabled()) return static_cast<uint32_t>(millis() / 1000);
    auto dt = M5.Rtc.getDateTime();
    struct tm t = {};
    t.tm_year = dt.date.year - 1900;
    t.tm_mon = dt.date.month - 1;
    t.tm_mday = dt.date.date;
    t.tm_hour = dt.time.hours;
    t.tm_min = dt.time.minutes;
    t.tm_sec = dt.time.seconds;
    setenv("TZ", "UTC0", 1);
    tzset();
    return static_cast<uint32_t>(mktime(&t));
}

bool clockValid() {
    if (!M5.Rtc.isEnabled()) return false;
    return M5.Rtc.getDateTime().date.year >= 2024;
}

void clockSet(uint32_t unixSeconds) {
    time_t t = unixSeconds;
    struct tm tmv;
    gmtime_r(&t, &tmv);
    M5.Rtc.setDateTime(&tmv);
}

bool lightSleep(int64_t wakeAtUs) {
    for (int p : kPins) gpio_wakeup_enable(static_cast<gpio_num_t>(p), GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    if (wakeAtUs >= 0) {
        const int64_t now = esp_timer_get_time();
        esp_sleep_enable_timer_wakeup(wakeAtUs > now ? wakeAtUs - now : 1000);
    }
    Serial.flush();
    esp_light_sleep_start();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    for (int p : kPins) gpio_wakeup_disable(static_cast<gpio_num_t>(p));
    return true;
}

void powerOff() {
    M5.Display.waitDisplay();
    M5.Display.sleep();
    M5.Power.powerOff();  // on battery this cuts power; on USB it may return
    // Still powered (USB): wait for the side button to be let go and pressed again;
    // the app then restarts.
    while (digitalRead(kPins[kSide]) == LOW) delay(20);
    while (digitalRead(kPins[kSide]) == HIGH) delay(20);
}

const char* name() { return "CoreInk"; }

}  // namespace board
