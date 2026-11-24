// M5Stack CoreInk (current revision, GDEY0154D67 screen). Pins from M5Stack's CoreInk
// library; what's been confirmed on a device is in docs/SPEC.md section 2.
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
// The board's power latch: high keeps the CoreInk on once the side button is let go. Light sleep
// lets go of the pins unless they're held, which switched the device off at its first sleep.
constexpr gpio_num_t kPowerHoldPin = GPIO_NUM_12;
constexpr int kBuzzerPin = 2;
constexpr int kBuzzerChannel = 0;

uint8_t region[(stoprock::Canvas::W / 8) * stoprock::Canvas::H];

}  // namespace

void begin() {
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    setCpuFrequencyMhz(80);  // plenty for this app; roughly halves the current while awake
    pinMode(kPowerHoldPin, OUTPUT);  // M5Unified sets it too; make sure: the device stays on
    digitalWrite(kPowerHoldPin, HIGH);
    M5.Display.setAutoDisplay(false);
    M5.Display.setRotation(0);

    // GPIO 34-39 (the rocker) can't pull up and the board has resistors for them; the top and
    // side buttons (GPIO 5, 27) need the chip's own pull-ups, as M5Stack's library sets them:
    // without, they float low, read as held from the start, and never register.
    for (int p : kPins) pinMode(p, p >= 34 ? INPUT : INPUT_PULLUP);
    delay(2);  // let the pull-ups settle before anything reads the buttons
    Serial.printf("buttons at start (1 = up, 0 = pressed): top %d, side %d, up %d, press %d, down %d\n",
                  digitalRead(kPins[kTop]), digitalRead(kPins[kSide]), digitalRead(kPins[kUp]),
                  digitalRead(kPins[kPress]), digitalRead(kPins[kDown]));
    pinMode(kLedPin, OUTPUT);
    digitalWrite(kLedPin, HIGH);
    ledcSetup(kBuzzerChannel, 2000, 8);
    ledcAttachPin(kBuzzerPin, kBuzzerChannel);
    ledcWrite(kBuzzerChannel, 0);
}

bool held(Button b) { return digitalRead(kPins[b]) == LOW; }
bool heldAtStart(Button b) { return held(b); }
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

bool rtcSeconds(uint32_t* seconds) {
    if (!M5.Rtc.isEnabled()) return false;
    *seconds = clockNow();
    return true;
}

bool command(char*, size_t) { return false; }  // commands come over USB only
bool startsOff() { return false; }               // running means it was switched on

bool lightSleep(int64_t wakeAtUs) {
    // Waking on a button needs a low-level trigger, and gpio_wakeup_enable writes it over the
    // falling edge the timing buttons interrupt on (gpio_wakeup_disable doesn't put it back). Left
    // like that, a press interrupts without end for as long as it's held, the app never gets to
    // see it held, and it's dropped. So: no interrupts while asleep, and the edge back after.
    for (Button b : {kTop, kSide}) gpio_intr_disable(static_cast<gpio_num_t>(kPins[b]));
    for (int p : kPins) gpio_wakeup_enable(static_cast<gpio_num_t>(p), GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    if (wakeAtUs >= 0) {
        const int64_t now = esp_timer_get_time();
        esp_sleep_enable_timer_wakeup(wakeAtUs > now ? wakeAtUs - now : 1000);
    }
    Serial.flush();
    gpio_hold_en(kPowerHoldPin);  // keep the power latch on through the sleep (or the board switches off)
    esp_light_sleep_start();
    gpio_hold_dis(kPowerHoldPin);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    for (int p : kPins) gpio_wakeup_disable(static_cast<gpio_num_t>(p));
    for (Button b : {kTop, kSide}) {  // (a press that woke us is the app's to see, from the level)
        gpio_set_intr_type(static_cast<gpio_num_t>(kPins[b]), GPIO_INTR_NEGEDGE);
        gpio_intr_enable(static_cast<gpio_num_t>(kPins[b]));
    }
    return true;
}

// Not M5.Power.powerOff(): on the CoreInk that ends in a deep sleep nothing wakes (M5Unified arms the
// side button only on boards without a real-time clock interrupt pin), so on USB, which keeps the board
// powered with the latch dropped, only a reset brought it back.
void powerOff() {
    const auto side = static_cast<gpio_num_t>(kPins[kSide]);
    M5.Display.waitDisplay();
    M5.Display.sleep();
    // Let go first (up for 50 ms): the button itself keeps the power on, and would wake the sleep below.
    for (int up = 0; up < 5; delay(10)) up = held(kSide) ? 0 : up + 1;
    Serial.println("power: latch dropped");
    Serial.flush();
    gpio_hold_dis(kPowerHoldPin);
    digitalWrite(kPowerHoldPin, LOW);  // on battery the power goes here
    delay(500);
    // Still powered (USB): sleep until the side button is pressed; the app then restarts.
    Serial.println("power: still powered (USB), waiting for the side button");
    Serial.flush();
    gpio_hold_en(kPowerHoldPin);  // the latch stays dropped: unplugging USB now switches it off
    gpio_intr_disable(static_cast<gpio_num_t>(kPins[kTop]));
    gpio_intr_disable(side);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    gpio_wakeup_enable(side, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    do {
        esp_light_sleep_start();
        delay(20);
    } while (!held(kSide));  // a bounce, not a press
    gpio_hold_dis(kPowerHoldPin);
    digitalWrite(kPowerHoldPin, HIGH);  // on again
}

const char* name() { return "CoreInk"; }

}  // namespace board
