// stoprock application: buttons, timing, shots, games, sounds, sleep and the display task.
// Behaviour follows docs/SPEC.md; the hardware is behind board.h.
#include <Arduino.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <time.h>

#include "board.h"
#include "databank.h"
#include "screens.h"
#include "shots.h"
#include "timing.h"

using namespace stoprock;

namespace {

constexpr int64_t kSec = 1000000;
constexpr int64_t kConfirmUs = 15000;          // a timing press must still be held 15 ms after its edge
constexpr int64_t kEdgeGapUs = 30000;          // ignore further edges of the same button within 30 ms
constexpr int64_t kStableUs = 20000;           // rocker debounce
constexpr int64_t kUndoHoldUs = kSec / 2;   // 0.5 s
constexpr int64_t kPowerHoldUs = 3 * kSec;
constexpr int64_t kIdleSleepUs = 20 * kSec;
constexpr int64_t kPowerOffUs = 5 * 60 * kSec;
constexpr int64_t kCleanAfterHhUs = 5 * kSec;
constexpr uint8_t kStonesPerClean = 4;         // ghost-clearing full refresh after every 4th timed stone
constexpr int64_t kBatteryCheckUs = 30 * kSec;
constexpr int kMaxStones = 12;

const char* const kTips[][2] = {
    {"Rocker up: made shot", "Rocker down: missed"},
    {"Hold rocker to undo", "keep holding to undo more"},
    {"Click at back line, hog", "and far hog for BH + HH"},
    {"Back2Hog: back to hog", "Hog2Hog: hog to far hog"},
    {"Top or side button both", "time. Use either one."},
    {"Two clicks within 1.5 s", "count as one"},
    {"After an HH, the next", "click starts a new stone"},
    {"Each switch-on starts", "a new game"},
    {"Switches off after 5 min", "idle. Hold side to wake"},
};
constexpr int kTipCount = sizeof kTips / sizeof kTips[0];
const char* const kWarning[2] = {"Battery low", "Charge after this game"};

// ------------------------------------------------------------------ sounds
struct Note { uint16_t hz, ms; };
const Note kClick[] = {{2400, 30}};
const Note kMade[] = {{3000, 70}};
const Note kMissed[] = {{1000, 110}};
const Note kUndo[] = {{1800, 60}, {0, 25}, {1200, 80}};
const Note kError[] = {{300, 180}};

struct Sound {
    const Note* notes = nullptr;
    uint8_t count = 0, index = 0;
    int64_t until = 0;

    template <size_t N> void play(const Note (&n)[N]) {
        notes = n;
        count = N;
        index = 0;
        board::tone(n[0].hz);
        until = esp_timer_get_time() + n[0].ms * 1000LL;
    }
    bool playing() const { return notes != nullptr; }
    void update(int64_t now) {
        if (!notes || now < until) return;
        if (++index >= count) { notes = nullptr; board::tone(0); return; }
        board::tone(notes[index].hz);
        until = now + notes[index].ms * 1000LL;
    }
} sound;

// ------------------------------------------------------------------ buttons
struct ButtonState {
    bool down = false;
    bool rawDown = false;
    int64_t rawSince = 0;
    int64_t downSince = 0;
    bool holdFired = false;
    int64_t nextRepeat = 0;   // rocker press: when the next undo fires while held
    bool pending = false;     // timing buttons: edge seen, waiting to confirm
    int64_t pendingT = 0;
};
ButtonState buttons[board::kButtonCount];

struct Edge { uint8_t button; int64_t t; };
QueueHandle_t edgeQueue;
volatile int64_t lastEdge[board::kButtonCount];

void IRAM_ATTR onEdge(void* arg) {
    const int64_t t = esp_timer_get_time();
    const uint8_t b = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(arg));
    if (t - lastEdge[b] < kEdgeGapUs) return;
    lastEdge[b] = t;
    Edge e{b, t};
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(edgeQueue, &e, &woken);
    if (woken) portYIELD_FROM_ISR();
}

bool isTimingButton(uint8_t b) { return b == board::kTop || b == board::kSide; }

// ------------------------------------------------------------------ state
TimingEngine timing;
ShotLog shots;
Stone stones[kMaxStones];
uint8_t stoneCount = 0;

int64_t lastActivity = 0;
int64_t sleepAt = 0;
int64_t refreshAt = -1;          // after a stone: next tip, and every 4th stone a full refresh
bool refreshFull = false;
uint8_t stonesSinceClean = 0;
int64_t nextBatteryCheck = 0;
bool lowBattery = false;
uint8_t tipIndex = 0;

// settings (tip position) in the default NVS partition
Preferences prefs;

// The game databank, in its own NVS partition ("games", see partitions.csv).
class NvsStore : public KeyValueStore {
public:
    bool begin() {
        if (p_.begin("bank", false, "games")) return true;
        // Never-used or unreadable partition: format it once and try again.
        nvs_flash_erase_partition("games");
        return p_.begin("bank", false, "games");
    }
    size_t get(const char* key, void* buf, size_t len) override {
        if (!p_.isKey(key)) return 0;
        const size_t n = p_.getBytesLength(key);
        if (n == 0 || n > len) return 0;
        return p_.getBytes(key, buf, n);
    }
    bool put(const char* key, const void* data, size_t len) override { return p_.putBytes(key, data, len) == len; }
    bool remove(const char* key) override { return p_.remove(key); }

private:
    Preferences p_;
};
NvsStore bankStore;
Databank bank(bankStore);

// Every switch-on starts a new game; it's stored once it has a shot or a time in it.
void newGame() {
    bank.startGame(board::clockNow(), board::clockValid());
    shots.clear();
    stoneCount = 0;
    Serial.println("game: new");
}

void saveShots() { bank.setShots(shots.count(), shots.bits(), board::clockNow()); }

uint16_t toStored(int32_t hundredths) {
    return hundredths < 0 || hundredths >= StoneTimes::kNoTime ? StoneTimes::kNoTime : static_cast<uint16_t>(hundredths);
}

// ------------------------------------------------------------------ display task
struct Model {
    Stone stones[kMaxStones];
    uint8_t stoneCount = 0;
    ShotLog shots;          // the shots on screen (deferred while a stone is timed)
    uint8_t tip = 0;
    bool warning = false;
    bool help = false;
    bool full = false;      // next update is a ghost-clearing full refresh
    char lastGame[40] = "";
};

Model model;
SemaphoreHandle_t modelLock;
TaskHandle_t displayTaskHandle;
volatile bool displayBusy = false;

void publish(void (*edit)(Model&)) {
    xSemaphoreTake(modelLock, portMAX_DELAY);
    edit(model);
    displayBusy = true;
    xSemaphoreGive(modelLock);
    xTaskNotifyGive(displayTaskHandle);
}

void displayTask(void*) {
    static Canvas shown, next;
    shown.clear();
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        Model m;
        xSemaphoreTake(modelLock, portMAX_DELAY);
        m = model;
        model.full = false;
        xSemaphoreGive(modelLock);

        if (m.help) {
            drawHelp(next, m.lastGame);
        } else {
            MainView v;
            v.stones = m.stones;
            v.stoneCount = m.stoneCount;
            v.shots = &m.shots;
            v.tip1 = m.warning ? kWarning[0] : kTips[m.tip][0];
            v.tip2 = m.warning ? kWarning[1] : kTips[m.tip][1];
            v.warning = m.warning;
            drawMain(next, v);
        }
        const int64_t t0 = esp_timer_get_time();
        if (m.full) {
            board::show(next, Rect{0, 0, Canvas::W, Canvas::H}, true);
        } else {
            Rect r = next.diff(shown);
            if (!r.empty()) board::show(next, r, false);
        }
        Serial.printf("display: %s %lld ms\n", m.full ? "full" : "partial", (esp_timer_get_time() - t0) / 1000);
        shown = next;
        if (ulTaskNotifyTake(pdTRUE, 0) > 0) xTaskNotifyGive(displayTaskHandle);  // more work queued
        else displayBusy = false;
    }
}

// Copy the app's state into the screen model. Shots are held back while a stone is timed.
void syncModel(bool full = false) {
    static bool sFull;
    sFull = full;
    publish([](Model& m) {
        memcpy(m.stones, stones, sizeof stones);
        m.stoneCount = stoneCount;
        if (!timing.active()) m.shots = shots;
        m.tip = tipIndex;
        m.warning = lowBattery;
        m.help = false;
        m.full = m.full || sFull;
    });
}

// ------------------------------------------------------------------ timing
void pushStone(const Stone& s) {
    if (stoneCount < kMaxStones) ++stoneCount;
    memmove(&stones[1], &stones[0], sizeof(Stone) * (stoneCount - 1));
    stones[0] = s;
}

void closeStone(int64_t now) {
    board::led(false);
    sleepAt = now;
}

// A timed stone finished (with an HH, or a BH only). At the safe moment, delay after it
// closed, the tip moves on with a partial update; every kStonesPerClean-th stone that update
// is a ghost-clearing full refresh instead.
void countTimedStone(int64_t now, int64_t delay) {
    ++stonesSinceClean;
    refreshAt = now + delay;
    refreshFull = stonesSinceClean >= kStonesPerClean;
}

void handleOutcome(const Outcome& o, int64_t now) {
    if (o.closedFirst == TimingEvent::BhClosed) {
        Serial.println("timing: BH-only stone closed");
        countTimedStone(now, 0);
    }
    switch (o.event) {
    case TimingEvent::Started:
        board::led(true);
        Serial.println("timing: start");
        break;
    case TimingEvent::Restarted:
        Serial.println("timing: double press, restart");
        break;
    case TimingEvent::BH:
        pushStone(Stone{toHundredths(o.us), -1});
        bank.addStone(toStored(stones[0].bh), StoneTimes::kNoTime, board::clockNow());
        Serial.printf("timing: BH %.2f\n", o.us / 1e6);
        syncModel();
        break;
    case TimingEvent::HH:
        if (o.hadBh && stoneCount > 0) {
            stones[0].hh = toHundredths(o.us);
            bank.setLastHogToHog(toStored(stones[0].hh), board::clockNow());
        } else {
            pushStone(Stone{-1, toHundredths(o.us)});
            bank.addStone(StoneTimes::kNoTime, toStored(stones[0].hh), board::clockNow());
        }
        Serial.printf("timing: HH %.2f\n", o.us / 1e6);
        closeStone(now);
        countTimedStone(now, kCleanAfterHhUs);
        syncModel();
        break;
    case TimingEvent::IgnoredTooSoon:
        Serial.println("timing: too soon after BH, ignored");
        break;
    case TimingEvent::Abandoned:
        Serial.println("timing: abandoned");
        closeStone(now);
        syncModel();  // releases any deferred shots
        break;
    case TimingEvent::BhClosed:
        Serial.println("timing: BH-only stone closed");
        closeStone(now);
        countTimedStone(now, 0);
        syncModel();
        break;
    case TimingEvent::None:
        break;
    }
}

// ------------------------------------------------------------------ power
void showHelpAndPowerOff() {
    static char line[40];
    static GameRecord g;  // big (128 stones): keep it off the stack
    if (!bank.lastGame(g)) {
        snprintf(line, sizeof line, "No games yet");
    } else {
        char date[12] = "";
        if (g.clockSet && g.started > 0) {
            time_t t = g.started;
            struct tm tmv;
            gmtime_r(&t, &tmv);
            strftime(date, sizeof date, "%d %b", &tmv);
        }
        ShotLog s;
        s.restore(g.shotCount, g.shotBits);
        if (s.count() == 0) snprintf(line, sizeof line, "Last game %s   %u stones", date, g.stoneCount);
        else snprintf(line, sizeof line, "Last game %s   %d/%d   %d%%", date, s.madeCount(), s.count(), s.percent());
    }
    Serial.println("power: off");
    publish([](Model& m) {
        strncpy(m.lastGame, line, sizeof m.lastGame - 1);
        m.help = true;
        m.full = true;
    });
    while (displayBusy) delay(10);
    board::led(false);
    board::tone(0);
    board::powerOff();
    ESP.restart();  // only reached where power-off isn't possible (USB, simulator)
}

// ------------------------------------------------------------------ input actions
void activity(int64_t now) {
    lastActivity = now;
    if (sleepAt < now + kIdleSleepUs) sleepAt = now + kIdleSleepUs;
}

void onTimingPress(int64_t t) {
    const Outcome o = timing.click(t);
    if (o.event == TimingEvent::IgnoredTooSoon) sound.play(kError);
    else sound.play(kClick);
    handleOutcome(o, t);
}

void onShot(bool made) {
    if (!shots.add(made)) { sound.play(kError); return; }
    if (made) sound.play(kMade); else sound.play(kMissed);
    Serial.printf("shots: %s, %d/%d\n", made ? "made" : "missed", shots.madeCount(), shots.count());
    saveShots();
    if (!timing.active()) syncModel();
}

// Returns false when there was nothing to undo.
bool onUndo() {
    if (!shots.undo()) { sound.play(kError); return false; }
    sound.play(kUndo);
    Serial.printf("shots: undo, %d/%d\n", shots.madeCount(), shots.count());
    saveShots();
    if (!timing.active()) syncModel();
    return true;
}

void onPress(uint8_t b, int64_t t) {
    ButtonState& s = buttons[b];
    s.down = true;
    s.downSince = t;
    s.holdFired = false;
    s.nextRepeat = t + kUndoHoldUs;
    activity(t);
    switch (b) {
    case board::kTop:
    case board::kSide: onTimingPress(t); break;
    case board::kUp: onShot(true); break;
    case board::kDown: onShot(false); break;
    default: break;  // rocker press acts on hold only
    }
}

void pollButtons(int64_t now) {
    // timing buttons: edges from the interrupt, confirmed by the level 15 ms later
    Edge e;
    while (xQueueReceive(edgeQueue, &e, 0) == pdTRUE) {
        ButtonState& s = buttons[e.button];
        if (!s.down && !s.pending) { s.pending = true; s.pendingT = e.t; }
    }
    for (uint8_t b = 0; b < board::kButtonCount; ++b) {
        ButtonState& s = buttons[b];
        const bool raw = board::held(static_cast<board::Button>(b));
        if (raw != s.rawDown) { s.rawDown = raw; s.rawSince = now; }
        const bool stable = now - s.rawSince >= kStableUs;

        if (isTimingButton(b)) {
            if (s.pending && now - s.pendingT >= kConfirmUs) {
                s.pending = false;
                if (raw) onPress(b, s.pendingT);  // the press time is the edge, not now
            }
            if (s.down && !raw && stable) s.down = false;
        } else {
            if (stable && raw && !s.down) onPress(b, s.rawSince);
            if (stable && !raw && s.down) s.down = false;
        }

        // holds
        if (s.down && !s.holdFired) {
            // Rocker press: undo after 0.5 s, then again every 0.5 s while held. Stops after
            // one error buzz once there's nothing left to undo.
            if (b == board::kPress && now >= s.nextRepeat) {
                s.nextRepeat += kUndoHoldUs;
                activity(now);
                if (!onUndo()) s.holdFired = true;
            }
            if (b == board::kSide && now - s.downSince >= kPowerHoldUs) { s.holdFired = true; showHelpAndPowerOff(); }
        }
    }
}

// Databank export, framed so an app can parse it: "@GAMES n", n JSON lines, "@END".
void printGames(uint32_t fromId) {
    static GameRecord g;
    static char json[2400];
    uint32_t n = 0;
    for (uint32_t id = fromId; id <= bank.lastId(); ++id) n += bank.load(id, g) ? 1 : 0;
    Serial.printf("@GAMES %lu\n", static_cast<unsigned long>(n));
    for (uint32_t id = fromId; id <= bank.lastId(); ++id) {
        if (!bank.load(id, g)) continue;
        Databank::toJson(g, json, sizeof json);
        Serial.println(json);
    }
    Serial.println("@END");
}

void serialCommands() {
    static char buf[48];
    static uint8_t len = 0;
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c != '\n' && c != '\r') { if (len < sizeof buf - 1) buf[len++] = c; continue; }
        buf[len] = 0;
        len = 0;
        if (buf[0] == 't' && buf[1] == ' ') {
            board::clockSet(strtoul(buf + 2, nullptr, 10));
            Serial.printf("clock: set to %lu\n", static_cast<unsigned long>(board::clockNow()));
        } else if (!strcmp(buf, "games")) {
            printGames(bank.firstId());
        } else if (!strcmp(buf, "games new")) {
            printGames(bank.syncedId() + 1 > bank.firstId() ? bank.syncedId() + 1 : bank.firstId());
        } else if (!strncmp(buf, "game ", 5)) {
            printGames(strtoul(buf + 5, nullptr, 10));  // from that id on
        } else if (!strncmp(buf, "ack ", 4)) {
            bank.markSynced(strtoul(buf + 4, nullptr, 10));
            Serial.printf("@ACK %lu\n", static_cast<unsigned long>(bank.syncedId()));
        } else if (!strcmp(buf, "bank")) {
            Serial.printf("@BANK {\"first\":%lu,\"last\":%lu,\"synced\":%lu,\"current\":%lu,\"version\":%u}\n",
                          static_cast<unsigned long>(bank.firstId()), static_cast<unsigned long>(bank.lastId()),
                          static_cast<unsigned long>(bank.syncedId()), static_cast<unsigned long>(bank.current().id),
                          Databank::kVersion);
        } else if (!strcmp(buf, "erase-all yes")) {
            bank.eraseAll();
            Serial.println("@ERASED");
        } else if (!strcmp(buf, "n")) {
            newGame();
            syncModel();
        } else if (!strcmp(buf, "o")) {
            showHelpAndPowerOff();
        } else if (!strcmp(buf, "s")) {
            Serial.printf("status: board %s, stones %d, shots %d/%d, games %lu-%lu, battery %d%%, clock %lu%s\n",
                          board::name(), stoneCount, shots.madeCount(), shots.count(),
                          static_cast<unsigned long>(bank.firstId()), static_cast<unsigned long>(bank.lastId()),
                          board::batteryPercent(), static_cast<unsigned long>(board::clockNow()),
                          board::clockValid() ? "" : " (not set)");
        } else if (buf[0]) {
            Serial.println("commands: t <unix seconds> | s | games | games new | game <id> | ack <id> | bank | "
                           "erase-all yes | n (new game) | o (power off)");
        }
    }
}

}  // namespace

// ------------------------------------------------------------------ Arduino entry points
void setup() {
    Serial.begin(115200);
    board::begin();
    edgeQueue = xQueueCreate(16, sizeof(Edge));
    modelLock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(displayTask, "display", 8192, nullptr, 1, &displayTaskHandle, 0);

    prefs.begin("stoprock", false);
    tipIndex = prefs.getUChar("tip", 0) % kTipCount;
    if (!bankStore.begin()) Serial.println("databank: no \"games\" partition, games won't be kept");
    bank.begin();
    newGame();  // every switch-on starts a new game

    // A button already held at start-up (the side button used to switch on) is neither a
    // press nor a hold: it does nothing until it has been let go.
    for (uint8_t b = 0; b < board::kButtonCount; ++b) {
        const bool held = board::held(static_cast<board::Button>(b));
        buttons[b].rawDown = buttons[b].down = held;
        buttons[b].holdFired = held;
    }
    attachInterruptArg(board::pin(board::kTop), onEdge, reinterpret_cast<void*>(board::kTop), FALLING);
    attachInterruptArg(board::pin(board::kSide), onEdge, reinterpret_cast<void*>(board::kSide), FALLING);

    const int64_t now = esp_timer_get_time();
    activity(now);
    syncModel(true);
    Serial.printf("stoprock ready (%s)\n", board::name());
}

void loop() {
    int64_t now = esp_timer_get_time();
    pollButtons(now);
    handleOutcome(timing.poll(now), now);
    sound.update(now);
    serialCommands();

    // between stones: next tip (partial update), or a ghost-clearing full refresh every
    // kStonesPerClean stones. If the next stone has already started, it waits for that
    // stone to end, which schedules it again.
    if (refreshAt >= 0 && now >= refreshAt) {
        refreshAt = -1;
        if (!timing.active()) {
            if (refreshFull) stonesSinceClean = 0;
            tipIndex = (tipIndex + 1) % kTipCount;
            prefs.putUChar("tip", tipIndex);
            syncModel(refreshFull);
        }
    }

    if (now >= nextBatteryCheck) {
        nextBatteryCheck = now + kBatteryCheckUs;
        const int pct = board::batteryPercent();
        const bool low = pct >= 0 && (lowBattery ? pct < 20 : pct < 15);
        if (low != lowBattery) { lowBattery = low; if (!timing.active()) syncModel(); }
    }

    if (now - lastActivity >= kPowerOffUs) showHelpAndPowerOff();

    // light sleep when nothing is going on
    bool anyDown = false;
    for (const ButtonState& s : buttons) anyDown |= s.down || s.pending || s.rawDown;
    if (!timing.active() && !displayBusy && !anyDown && !sound.playing() && now >= sleepAt) {
        int64_t wake = lastActivity + kPowerOffUs;
        if (refreshAt >= 0 && refreshAt < wake) wake = refreshAt;
        if (board::lightSleep(wake)) {
            now = esp_timer_get_time();
            // A timing button that woke us: its press time is the wake-up time.
            for (uint8_t b : {board::kTop, board::kSide}) {
                if (board::held(static_cast<board::Button>(b)) && !buttons[b].pending) {
                    lastEdge[b] = now;
                    buttons[b].pending = true;
                    buttons[b].pendingT = now;
                }
            }
            sleepAt = now + kIdleSleepUs;
            return;
        }
    }
    delay(2);
}
