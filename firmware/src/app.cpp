// stoprock application: buttons, timing, shots, games, sounds, sleep and the display task.
// Behaviour follows docs/SPEC.md; the hardware is behind board.h.
#include <Arduino.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <time.h>

#include "board.h"
#include "screens.h"
#include "shots.h"
#include "timing.h"

using namespace stoprock;

namespace {

constexpr int64_t kSec = 1000000;
constexpr int64_t kConfirmUs = 15000;          // a timing press must still be held 15 ms after its edge
constexpr int64_t kEdgeGapUs = 30000;          // ignore further edges of the same button within 30 ms
constexpr int64_t kStableUs = 20000;           // rocker debounce
constexpr int64_t kUndoHoldUs = 1 * kSec;
constexpr int64_t kPowerHoldUs = 3 * kSec;
constexpr int64_t kIdleSleepUs = 20 * kSec;
constexpr int64_t kPowerOffUs = 5 * 60 * kSec;
constexpr int64_t kCleanAfterHhUs = 5 * kSec;
constexpr int64_t kBatteryCheckUs = 30 * kSec;
constexpr uint32_t kNewGameGapS = 2 * 3600;
constexpr int kMaxStones = 12;
constexpr int kMaxGames = 64;

const char* const kTips[][2] = {
    {"Rocker up: made shot", "Rocker down: missed"},
    {"Hold rocker 1 s to undo", "your last shot"},
    {"Click at back line, hog", "and far hog for BH + HH"},
    {"BH: back line to hog", "HH: hog line to hog line"},
    {"Ring marks the slot your", "next shot will fill"},
    {"Top or side button both", "time. Use either one."},
    {"Two clicks within 1.5 s", "count as one"},
    {"After an HH, the next", "click starts a new stone"},
    {"% is made shots out of", "rocks thrown tonight"},
    {"New game: hold rocker", "while switching on"},
    {"Rated to 0 \xC2\xB0" "C. Keep it", "off the ice surface"},
    {"Charge at room temp,", "never at the rink"},
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
int64_t cleanAt = -1;
int64_t nextBatteryCheck = 0;
bool lowBattery = false;
uint8_t tipIndex = 0;

// games, persisted
struct GameRec { uint32_t date; uint8_t count; uint8_t pad; uint16_t bits; };
Preferences prefs;
GameRec games[kMaxGames];
uint16_t gameTotal = 0;   // games ever started; current = gameTotal - 1
uint32_t lastEntry = 0;   // clock seconds of the last entry

GameRec& currentGame() { return games[(gameTotal - 1) % kMaxGames]; }

void saveGames() {
    prefs.putBytes("games", games, sizeof games);
    prefs.putUShort("ngames", gameTotal);
    prefs.putULong("last", lastEntry);
}

void loadGames() {
    prefs.begin("stoprock", false);
    if (prefs.isKey("games") && prefs.getBytesLength("games") == sizeof games) prefs.getBytes("games", games, sizeof games);
    gameTotal = prefs.getUShort("ngames", 0);
    lastEntry = prefs.getULong("last", 0);
    tipIndex = prefs.getUChar("tip", 0) % kTipCount;
}

void newGame() {
    ++gameTotal;
    currentGame() = GameRec{board::clockNow(), 0, 0, 0};
    shots.clear();
    stoneCount = 0;
    saveGames();
    Serial.println("game: new");
}

void saveShots() {
    if (gameTotal == 0) return;
    currentGame().count = shots.count();
    currentGame().bits = shots.bits();
    saveGames();
}

// Every entry (a time or a shot) may start a new game after a long gap.
void noteEntry() {
    const uint32_t now = board::clockNow();
    if (gameTotal == 0 || (now > lastEntry && now - lastEntry > kNewGameGapS)) newGame();
    lastEntry = now;
    prefs.putULong("last", lastEntry);
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

void closeStone(int64_t now, bool cleanNow) {
    board::led(false);
    sleepAt = now;
    if (cleanNow) cleanAt = now;
}

void handleOutcome(const Outcome& o, int64_t now) {
    if (o.closedFirst == TimingEvent::BhClosed) Serial.println("timing: BH-only stone closed");
    switch (o.event) {
    case TimingEvent::Started:
        board::led(true);
        Serial.println("timing: start");
        break;
    case TimingEvent::Restarted:
        Serial.println("timing: double press, restart");
        break;
    case TimingEvent::BH:
        noteEntry();
        pushStone(Stone{toHundredths(o.us), -1});
        Serial.printf("timing: BH %.2f\n", o.us / 1e6);
        syncModel();
        break;
    case TimingEvent::HH:
        noteEntry();
        if (o.hadBh && stoneCount > 0) stones[0].hh = toHundredths(o.us);
        else pushStone(Stone{-1, toHundredths(o.us)});
        Serial.printf("timing: HH %.2f\n", o.us / 1e6);
        closeStone(now, false);
        cleanAt = now + kCleanAfterHhUs;
        syncModel();
        break;
    case TimingEvent::IgnoredTooSoon:
        Serial.println("timing: too soon after BH, ignored");
        break;
    case TimingEvent::Abandoned:
        Serial.println("timing: abandoned");
        closeStone(now, false);
        syncModel();  // releases any deferred shots
        break;
    case TimingEvent::BhClosed:
        Serial.println("timing: BH-only stone closed");
        closeStone(now, true);
        syncModel();
        break;
    case TimingEvent::None:
        break;
    }
}

// ------------------------------------------------------------------ power
void showHelpAndPowerOff() {
    static char line[40];
    if (gameTotal == 0) {
        snprintf(line, sizeof line, "No games yet");
    } else {
        const GameRec& g = currentGame();
        char date[12] = "";
        if (board::clockValid() && g.date > 0) {
            time_t t = g.date;
            struct tm tmv;
            gmtime_r(&t, &tmv);
            strftime(date, sizeof date, "%d %b", &tmv);
        }
        ShotLog s;
        s.restore(g.count, g.bits);
        if (s.count() == 0) snprintf(line, sizeof line, "Last game %s   no shots", date);
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
    noteEntry();
    if (!shots.add(made)) { sound.play(kError); return; }
    if (made) sound.play(kMade); else sound.play(kMissed);
    Serial.printf("shots: %s, %d/%d\n", made ? "made" : "missed", shots.madeCount(), shots.count());
    saveShots();
    if (!timing.active()) syncModel();
}

void onUndo() {
    if (!shots.undo()) { sound.play(kError); return; }
    sound.play(kUndo);
    Serial.printf("shots: undo, %d/%d\n", shots.madeCount(), shots.count());
    saveShots();
    if (!timing.active()) syncModel();
}

void onPress(uint8_t b, int64_t t) {
    ButtonState& s = buttons[b];
    s.down = true;
    s.downSince = t;
    s.holdFired = false;
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
            if (b == board::kPress && now - s.downSince >= kUndoHoldUs) { s.holdFired = true; activity(now); onUndo(); }
            if (b == board::kSide && now - s.downSince >= kPowerHoldUs) { s.holdFired = true; showHelpAndPowerOff(); }
        }
    }
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
        } else if (!strcmp(buf, "n")) {
            newGame();
            syncModel();
        } else if (!strcmp(buf, "o")) {
            showHelpAndPowerOff();
        } else if (!strcmp(buf, "s")) {
            Serial.printf("status: board %s, stones %d, shots %d/%d, games %u, battery %d%%, clock %lu%s\n",
                          board::name(), stoneCount, shots.madeCount(), shots.count(), gameTotal,
                          board::batteryPercent(), static_cast<unsigned long>(board::clockNow()),
                          board::clockValid() ? "" : " (not set)");
        } else if (buf[0]) {
            Serial.println("commands: t <unix seconds> | n (new game) | o (power off) | s (status)");
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

    loadGames();
    // Holding the rocker press while switching on starts a new game.
    if (board::held(board::kPress) || gameTotal == 0) newGame();
    else shots.restore(currentGame().count, currentGame().bits);

    for (uint8_t b = 0; b < board::kButtonCount; ++b) buttons[b].rawDown = buttons[b].down = board::held(static_cast<board::Button>(b));
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

    // ghost clearing between stones; advances the tip
    if (cleanAt >= 0 && now >= cleanAt) {
        cleanAt = -1;
        if (!timing.active()) {
            tipIndex = (tipIndex + 1) % kTipCount;
            prefs.putUChar("tip", tipIndex);
            syncModel(true);
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
        if (cleanAt >= 0 && cleanAt < wake) wake = cleanAt;
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
