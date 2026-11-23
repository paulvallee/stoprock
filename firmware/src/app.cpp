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
constexpr int64_t kDoublePressUs = 400000;     // two rocker presses within 0.4 s: a double press
constexpr uint8_t kRocksPerPlayer = 2;         // each player throws two rocks an end
constexpr int64_t kFocusHoldUs = kSec / 2;     // league: hold rocker up/down this long to move the focus
constexpr int64_t kAwakeAfterPressUs = 2 * kSec;    // stay awake this long after a rocker press
constexpr int64_t kAwakeAfterWakeUs = 40000;        // time to confirm the button that woke us
constexpr int64_t kAuditEveryUs = 10 * 60 * kSec;   // clock audit sample interval
constexpr int64_t kPowerOffUs = 5 * 60 * kSec;
constexpr int64_t kCleanAfterHhUs = 5 * kSec;
constexpr uint8_t kStonesPerClean = 4;         // ghost-clearing full refresh after every 4th timed stone
#ifdef STOPROCK_BOARD_EMU
// Simulator: react to the battery slider quickly, and let 1-minute games count towards the
// games-left estimate so it can be tried without playing 20-minute games.
constexpr int64_t kBatteryCheckUs = 2 * kSec;
constexpr uint32_t kMinGameSecondsForBattery = 60;
#else
constexpr int64_t kBatteryCheckUs = 30 * kSec;
constexpr uint32_t kMinGameSecondsForBattery = 20 * 60;
#endif
constexpr int kMaxStones = 25;  // kept for the screen: at least a screenful of each player

const char* const kPositionNames[4] = {"lead", "second", "third", "skip"};

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
// Chosen at switch-on by what the rocker is doing while the side button is held:
// nothing = competition, down = league, up = coach (docs/SPEC.md section 2).
GameMode mode = GameMode::Competition;
uint8_t coachPage = 0;          // coach: 0 timer priority, 1 rock priority
// Competition and league: you time your teammates' stones, a column each. The cursor is on the
// player about to throw; it moves on by itself after their second rock. In competition the
// rocker moves it too; in league, logging your own shot puts it on the player after you.
uint8_t myPosition = 0;         // 0 lead .. 3 skip (double press of the rocker centre; kept)
uint8_t activeCol = 0;          // the column (0-2) with the cursor
uint8_t rocksForActive = 0;     // stones timed for that player since the cursor got there
// League: what rocker up/down work on, the focus (bold border): your rocks (made/missed) or the
// players (up = next, down = previous). Hold up 0.5 s for the players, hold down 0.5 s for your rocks.
Focus focus = Focus::Rocks;
int64_t lastRockerPress = -kSec; // coach: when the rocker was last pressed (short presses only count)

TimingEngine timing;
ShotLog shots;
Stone stones[kMaxStones];
uint8_t stoneCount = 0;

int64_t lastActivity = 0;
int64_t sleepAt = 0;
int64_t refreshAt = -1;          // after every 4th stone: a ghost-clearing full refresh
constexpr int64_t kTipUs = 5 * kSec;
int64_t tipUntil = -1;           // competition and league: the switch-on position tip shows until then
bool tipWaiting = false;         // ... its 5 s start once the screen showing it is up
uint8_t stonesSinceClean = 0;

// Sleeping while a stone is timed (setting "sleeptiming", default on). Light sleep stops the
// main crystal, so time asleep is counted by the chip's less precise backup clock. The clock
// audit measures how far that drifts, against the real-time clock's crystal, during real use.
bool sleepWhileTiming = true;
int64_t sleptUs = 0;            // total time in light sleep since switch-on (chip timer)
int64_t sleptAtAnchor = 0;      // sleptUs at the click the current interval is measured from

struct ClockAudit {
    uint32_t gameId = 0;
    uint8_t sleepWhileTiming = 1;
    uint8_t pad[3] = {};
    uint32_t samples = 0;
    uint32_t rtc0 = 0, rtcLast = 0;      // real-time clock seconds at the first and last sample
    int64_t timer0 = 0, timerLast = 0;   // chip timer at those second edges
    int64_t slept0 = 0, sleptLast = 0;   // sleptUs at those samples
    uint32_t results = 0;                // B2H and H2H times recorded
    int64_t resultUs = 0;                // their total length
    int64_t resultSleptUs = 0;           // ... of which the chip was asleep
};
ClockAudit audit;
int64_t nextAuditAt = 0;
constexpr int kAuditHistory = 8;
int64_t nextBatteryCheck = 0;
bool lowBattery = false;

// settings (your position, sleep while timing, clock audits) in the default NVS partition
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
    bank.startGame(board::clockNow(), board::clockValid(), board::batteryPercent(), mode);
    shots.setCapacity(mode == GameMode::Coach ? ShotLog::kCoachSlots : ShotLog::kLeagueSlots);
    activeCol = 0;
    rocksForActive = 0;
    focus = Focus::Rocks;
    // "Double click rocker to change position", for the first 5 s (not in coach: no positions)
    tipWaiting = mode != GameMode::Coach;
    tipUntil = -1;
    stoneCount = 0;
    Serial.printf("game: new, %s mode\n", modeName(mode));
}

void saveShots() { bank.setShots(shots.count(), shots.bits(), board::clockNow()); }

uint16_t toStored(int32_t hundredths) {
    return hundredths < 0 || hundredths >= StoneTimes::kNoTime ? StoneTimes::kNoTime : static_cast<uint16_t>(hundredths);
}

// ------------------------------------------------------------------ display task
struct Model {
    GameMode mode = GameMode::League;
    uint8_t page = 0;       // coach: which page
    uint8_t me = 0;         // competition: your position
    uint8_t activeCol = 0;  // competition: the column with the cursor
    Stone stones[kMaxStones];
    uint8_t stoneCount = 0;
    ShotLog shots;          // the shots on screen (deferred while a stone is timed)
    bool warning = false;
    bool help = false;
    bool full = false;      // next update is a ghost-clearing full refresh
    bool tip = false;       // the switch-on position tip
    Waiting waiting = Waiting::None;  // a stone being timed: the stopwatch shows where its time will go
    Focus focus = Focus::Rocks;       // league: what the rocker works on
    char lastGame[40] = "";
    int shotPct = -1;       // help page: last game's shot percentage
    int battery = -1;       // help page: charge at switch-off
    char batteryNote[28] = "";  // help page: games of battery left
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
            drawHelp(next, m.lastGame, m.shotPct, m.battery, m.batteryNote);
        } else if (m.mode == GameMode::Competition) {
            drawCompetition(next, m.stones, m.stoneCount, m.me, m.activeCol, m.warning, m.tip, m.waiting);
        } else if (m.mode == GameMode::Coach) {
            if (m.page == 0) drawCoachTimer(next, m.stones, m.stoneCount, m.shots, m.waiting);
            else drawCoachRocks(next, m.stones, m.stoneCount, m.shots, m.waiting);
        } else {
            drawLeague(next, m.stones, m.stoneCount, m.shots, m.me, m.activeCol, m.warning, m.tip, m.waiting, m.focus);
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
        m.mode = mode;
        m.page = coachPage;
        m.me = myPosition;
        m.activeCol = activeCol;
        memcpy(m.stones, stones, sizeof stones);
        m.stoneCount = stoneCount;
        if (!timing.active()) m.shots = shots;
        m.warning = lowBattery;
        m.help = false;
        m.full = m.full || sFull;
        m.tip = tipWaiting || tipUntil >= 0;
        m.focus = focus;
        m.waiting = timing.state() == TimingState::Started     ? Waiting::BackToHog
                    : timing.state() == TimingState::WaitingHH ? Waiting::HogToHog
                                                               : Waiting::None;
    });
}

// ------------------------------------------------------------------ timing
// A new stone, newest first. Competition files it in the active column (see addStone()).
void pushStone(Stone s) {
    s.player = mode == GameMode::Coach ? Stone::kNoPlayer : teammate(myPosition, activeCol);
    addStone(stones, stoneCount, kMaxStones, s);
}

void closeStone(int64_t now) {
    board::led(false);
    sleepAt = now;
}

// A timed stone finished (with an H2H, or a B2H only). Every kStonesPerClean-th stone gets a
// ghost-clearing full refresh at the safe moment, delay after it closed.
void countTimedStone(int64_t now, int64_t delay) {
    // After the player's second rock, the cursor moves on to the next player.
    if (mode != GameMode::Coach && stoneCount > 0 && stones[0].player == teammate(myPosition, activeCol) &&
        ++rocksForActive >= kRocksPerPlayer) {
        rocksForActive = 0;
        activeCol = (activeCol + 1) % kGridColumns;
        Serial.printf("timing: next player, %s\n", kPositionNames[teammate(myPosition, activeCol)]);
        syncModel();
    }
    // League: once a stone has been timed, the rocker goes back to your rocks (the players focus
    // is only for putting the cursor right). The screen update that follows shows it.
    if (mode == GameMode::League && focus == Focus::Players) {
        focus = Focus::Rocks;
        Serial.println("focus: rocks (after a timed stone)");
    }
    // every kStonesPerClean-th stone: a ghost-clearing full refresh once it's safe
    if (++stonesSinceClean >= kStonesPerClean) refreshAt = now + delay;
}

// A B2H or H2H was measured: note how much of it the chip spent asleep, for the clock audit.
void noteResult(int64_t us) {
    const int64_t asleep = sleptUs - sleptAtAnchor;
    ++audit.results;
    audit.resultUs += us;
    audit.resultSleptUs += asleep;
    sleptAtAnchor = sleptUs;  // the next interval (H2H after B2H) starts at this click
    Serial.printf("timing: %.0f%% of it asleep\n", us > 0 ? 100.0 * asleep / us : 0.0);
}

void handleOutcome(const Outcome& o, int64_t now) {
    if (o.closedFirst == TimingEvent::BhClosed) {
        Serial.println("timing: B2H-only stone closed");
        countTimedStone(now, 0);
    }
    switch (o.event) {
    case TimingEvent::Started:
        sleptAtAnchor = sleptUs;
        // no LED: the case covers it, and the stopwatch icon shows timing in progress
        Serial.println("timing: start");
        syncModel();  // a stopwatch where the time will appear
        break;
    case TimingEvent::Restarted:
        sleptAtAnchor = sleptUs;
        Serial.println("timing: double press, restart");
        break;
    case TimingEvent::BH:
        noteResult(o.us);
        pushStone(Stone{toHundredths(o.us), -1});
        bank.addStone(toStored(stones[0].bh), StoneTimes::kNoTime, board::clockNow(), stones[0].player);
        Serial.printf("timing: B2H %.2f\n", o.us / 1e6);
        syncModel();
        break;
    case TimingEvent::HH:
        noteResult(o.us);
        if (o.hadBh && stoneCount > 0) {
            stones[0].hh = toHundredths(o.us);
            bank.setLastHogToHog(toStored(stones[0].hh), board::clockNow());
        } else {
            pushStone(Stone{-1, toHundredths(o.us)});
            bank.addStone(StoneTimes::kNoTime, toStored(stones[0].hh), board::clockNow(), stones[0].player);
        }
        Serial.printf("timing: H2H %.2f\n", o.us / 1e6);
        closeStone(now);
        countTimedStone(now, kCleanAfterHhUs);
        syncModel();
        break;
    case TimingEvent::IgnoredTooSoon:
        Serial.println("timing: too soon after B2H, ignored");
        break;
    case TimingEvent::Abandoned:
        Serial.println("timing: abandoned");
        closeStone(now);
        syncModel();  // releases any deferred shots
        break;
    case TimingEvent::BhClosed:
        Serial.println("timing: B2H-only stone closed");
        closeStone(now);
        countTimedStone(now, 0);
        syncModel();
        break;
    case TimingEvent::None:
        break;
    }
}

// ------------------------------------------------------------------ clock audit
void auditSave() {
    audit.gameId = bank.current().id;
    audit.sleepWhileTiming = sleepWhileTiming;
    ClockAudit history[kAuditHistory] = {};
    if (prefs.getBytesLength("audits") == sizeof history) prefs.getBytes("audits", history, sizeof history);
    // slot 0 is this switch-on's audit: shift the others down when a new one starts
    static bool shifted = false;
    if (!shifted) {
        memmove(&history[1], &history[0], sizeof(ClockAudit) * (kAuditHistory - 1));
        shifted = true;
    }
    history[0] = audit;
    prefs.putBytes("audits", history, sizeof history);
}

// A sample pairs a real-time clock second edge with the chip timer. Waiting for the edge
// takes up to ~1 s, so it runs a step per loop pass (buttons keep working meanwhile) and the
// chip doesn't sleep until it's done.
bool auditWaiting = false;
uint32_t auditFrom = 0;
int64_t auditGiveUpAt = 0;

void auditRecord(uint32_t rtc, int64_t timer) {
    if (audit.samples == 0) {
        audit.rtc0 = rtc;
        audit.timer0 = timer;
        audit.slept0 = sleptUs;
    }
    audit.rtcLast = rtc;
    audit.timerLast = timer;
    audit.sleptLast = sleptUs;
    ++audit.samples;
    auditSave();
}

void auditStart(int64_t now) {
    if (board::rtcSeconds(&auditFrom)) {
        auditWaiting = true;
        auditGiveUpAt = now + 1500000;
    }
}

void auditStep() {
    if (!auditWaiting) return;
    const int64_t before = esp_timer_get_time();
    uint32_t sec;
    if (!board::rtcSeconds(&sec) || before > auditGiveUpAt) { auditWaiting = false; return; }
    if (sec != auditFrom) {  // ticked between the previous read and this one (a loop pass, ~2 ms)
        auditWaiting = false;
        auditRecord(sec, before);
    }
}

// Blocking version, for switch-off.
void auditSampleNow() {
    auditStart(esp_timer_get_time());
    while (auditWaiting) { auditStep(); delay(1); }
}

void printAudit(const ClockAudit& a, bool current) {
    const double span = static_cast<double>(a.rtcLast - a.rtc0);              // seconds
    const double errMs = ((a.timerLast - a.timer0) - span * 1e6) / 1000.0;    // timer minus RTC
    const double sleptS = (a.sleptLast - a.slept0) / 1e6;
    const double ppm = sleptS > 1 ? errMs * 1000.0 / sleptS : 0;               // error per second asleep
    const double asleepPct = a.resultUs > 0 ? 100.0 * a.resultSleptUs / a.resultUs : 0;
    Serial.printf("{\"current\":%s,\"game\":%lu,\"sleep_while_timing\":%s,\"samples\":%lu,"
                  "\"span_s\":%.0f,\"slept_s\":%.0f,\"timer_minus_rtc_ms\":%.1f,\"sleep_clock_ppm\":%.0f,"
                  "\"results\":%lu,\"results_asleep_pct\":%.1f,\"est_error_ms_15s_h2h\":%.1f}\n",
                  current ? "true" : "false", static_cast<unsigned long>(a.gameId),
                  a.sleepWhileTiming ? "true" : "false", static_cast<unsigned long>(a.samples), span, sleptS,
                  errMs, ppm, static_cast<unsigned long>(a.results), asleepPct,
                  15.0 * (asleepPct / 100.0) * ppm / 1000.0);
}

// ------------------------------------------------------------------ power
void showHelpAndPowerOff() {
    static char line[40];
    static char note[28];
    static int shotPct, battery;
    shotPct = -1;
    battery = board::batteryPercent();
    bank.endGame(battery, board::clockNow());  // log the battery at the end of this game
    const int left = bank.gamesLeft(battery, kMinGameSecondsForBattery);
    if (left < 0) note[0] = 0;
    else if (left == 0) snprintf(note, sizeof note, "Charge before next game");
    else snprintf(note, sizeof note, "~%d game%s battery left", left, left == 1 ? "" : "s");
    static GameRecord g;  // big (128 stones): keep it off the stack
    if (!bank.lastGame(g)) {
        snprintf(line, sizeof line, "No games yet");
    } else {
        ShotLog s(ShotLog::kMaxSlots);
        s.restore(g.shotCount, g.shotBits);
        if (s.count() == 0) snprintf(line, sizeof line, "Last game %u stones timed", g.stoneCount);
        else {
            snprintf(line, sizeof line, "Last game %d/%d shots made", s.madeCount(), s.count());
            shotPct = s.percent();
        }
    }
    auditSampleNow();
    Serial.println("power: off");
    publish([](Model& m) {
        strncpy(m.lastGame, line, sizeof m.lastGame - 1);
        m.shotPct = shotPct;
        m.battery = battery;
        strncpy(m.batteryNote, note, sizeof m.batteryNote - 1);
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
    if (sleepAt < now + kAwakeAfterPressUs) sleepAt = now + kAwakeAfterPressUs;
}

void onTimingPress(int64_t t) {
    const Outcome o = timing.click(t);
    if (o.event == TimingEvent::IgnoredTooSoon) sound.play(kError);
    else sound.play(kClick);
    handleOutcome(o, t);
    // Nothing to wait for between clicks: sleep as soon as the beep and screen are done.
    if (sleepWhileTiming || !timing.active()) sleepAt = t;
}

void onShot(bool made) {
    if (!shots.add(made)) { sound.play(kError); return; }
    if (made) sound.play(kMade); else sound.play(kMissed);
    Serial.printf("shots: %s, %d/%d\n", made ? "made" : "missed", shots.madeCount(), shots.count());
    if (mode == GameMode::League) {  // you just threw: the player after you is next
        const uint8_t next = (myPosition + 1) % 4;
        activeCol = next < myPosition ? next : next - 1;
        rocksForActive = 0;
    }
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

// Coach: a second short rocker press within 0.4 s switches page (and isn't an undo).
// The first press does nothing by itself, so switching page never costs a shot.
// Competition and league: a double press changes your position (lead, 2nd, 3rd, skip, round again).
void onRockerPress(ButtonState& s, int64_t t) {
    if (t - lastRockerPress < kDoublePressUs) {
        s.holdFired = true;      // this press can't turn into an undo
        lastRockerPress = -kSec; // a third press starts over
        sound.play(kClick);
        if (mode == GameMode::Coach) {
            coachPage ^= 1;
            Serial.printf("coach: page %d\n", coachPage + 1);
        } else {
            myPosition = (myPosition + 1) % 4;
            prefs.putUChar("mypos", myPosition);
            activeCol = 0;
            rocksForActive = 0;
            Serial.printf("position: you are %s\n", kPositionNames[myPosition]);
            tipWaiting = false;  // show "You are ..." (the switch-on tip) for 5 s
            tipUntil = t + kTipUs;
        }
        syncModel();  // shows at once, even while a stone is timed
        return;
    }
    lastRockerPress = t;
}

// The player cursor one column left (-1) or right (+1), round and round.
void movePlayer(int dir) {
    activeCol = (activeCol + (dir < 0 ? kGridColumns - 1 : 1)) % kGridColumns;
    rocksForActive = 0;
    sound.play(kClick);
    Serial.printf("timing: %s next\n", kPositionNames[teammate(myPosition, activeCol)]);
    syncModel();
}

// League: rocker up/down held 0.5 s moves the focus (up: players, down: your rocks).
void setFocus(Focus f) {
    focus = f;
    sound.play(kClick);
    Serial.printf("focus: %s\n", f == Focus::Players ? "players" : "rocks");
    syncModel();
}

// League: a short press of rocker up/down, acted on when it's let go (a long press moves the
// focus instead): made/missed, or next/previous player.
void onRelease(uint8_t b) {
    if (mode != GameMode::League || (b != board::kUp && b != board::kDown) || buttons[b].holdFired) return;
    if (focus == Focus::Rocks) onShot(b == board::kUp);
    else movePlayer(b == board::kUp ? 1 : -1);  // up = next player, down = previous
}

void onPress(uint8_t b, int64_t t) {
    ButtonState& s = buttons[b];
    s.down = true;
    s.downSince = t;
    s.holdFired = false;
    s.nextRepeat = t + kUndoHoldUs;
    // Competition: rocker up/down moves the cursor to the next/previous player (round and
    // round); a double press of the centre changes your position. No undo.
    if (mode == GameMode::Competition && !isTimingButton(b)) {
        s.holdFired = true;
        activity(t);
        if (b == board::kPress) { onRockerPress(s, t); return; }
        movePlayer(b == board::kUp ? 1 : -1);  // up = next player, down = previous
        return;
    }
    activity(t);
    // League: rocker up/down act when let go, or move the focus when held (pollButtons).
    if (mode == GameMode::League && (b == board::kUp || b == board::kDown)) return;
    switch (b) {
    case board::kTop:
    case board::kSide: onTimingPress(t); break;
    case board::kUp: onShot(true); break;
    case board::kDown: onShot(false); break;
    case board::kPress: onRockerPress(s, t); break;  // otherwise it acts on hold only
    default: break;
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
            if (stable && !raw && s.down) {
                onRelease(b);
                s.down = false;
            }
        }

        // holds
        if (s.down && !s.holdFired) {
            // Rocker press: undo after 0.5 s, then again every 0.5 s while held. Stops after
            // one error buzz once there's nothing left to undo.
            if (b == board::kPress && now >= s.nextRepeat) {
                s.nextRepeat += kUndoHoldUs;
                lastRockerPress = -kSec;  // a hold isn't half of a double press
                activity(now);
                if (!onUndo()) s.holdFired = true;
            }
            if (b == board::kSide && now - s.downSince >= kPowerHoldUs) { s.holdFired = true; showHelpAndPowerOff(); }
            if (mode == GameMode::League && (b == board::kUp || b == board::kDown) &&
                now - s.downSince >= kFocusHoldUs) {
                s.holdFired = true;  // not a short press when it's let go
                activity(now);
                setFocus(b == board::kUp ? Focus::Players : Focus::Rocks);
            }
        }
    }
}

// Databank export, framed so an app can parse it: "@GAMES n", n JSON lines, "@END".
void printGames(uint32_t fromId) {
    static GameRecord g;
    static char json[3600];  // a full coach game: 64 shots, 128 stones
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
        } else if (!strcmp(buf, "audit")) {
            ClockAudit history[kAuditHistory] = {};
            if (prefs.getBytesLength("audits") == sizeof history) prefs.getBytes("audits", history, sizeof history);
            Serial.println("@AUDIT");
            printAudit(audit, true);
            for (int i = 1; i < kAuditHistory; ++i)
                if (history[i].samples > 0) printAudit(history[i], false);
            Serial.println("@END");
        } else if (!strncmp(buf, "sleeptiming ", 12)) {
            sleepWhileTiming = !strcmp(buf + 12, "on");
            prefs.putBool("sleeptim", sleepWhileTiming);
            Serial.printf("sleep while timing: %s\n", sleepWhileTiming ? "on" : "off");
        } else if (!strcmp(buf, "n")) {
            newGame();
            syncModel();
        } else if (!strncmp(buf, "mode ", 5)) {  // testing: as if switched on in that mode
            const char* name = buf + 5;
            if (!strcmp(name, "competition")) mode = GameMode::Competition;
            else if (!strcmp(name, "league")) mode = GameMode::League;
            else if (!strcmp(name, "coach")) mode = GameMode::Coach;
            else { Serial.println("modes: competition | league | coach"); continue; }
            coachPage = 0;
            newGame();
            syncModel(true);
        } else if (!strcmp(buf, "page")) {  // testing: coach page switch, as a double press
            if (mode == GameMode::Coach) { coachPage ^= 1; syncModel(); }
        } else if (!strcmp(buf, "o")) {
            showHelpAndPowerOff();
        } else if (!strcmp(buf, "s")) {
            Serial.printf("status: board %s, %s mode, stones %d, shots %d/%d, games %lu-%lu, battery %d%%, clock %lu%s\n",
                          board::name(), modeName(mode), stoneCount, shots.madeCount(), shots.count(),
                          static_cast<unsigned long>(bank.firstId()), static_cast<unsigned long>(bank.lastId()),
                          board::batteryPercent(), static_cast<unsigned long>(board::clockNow()),
                          board::clockValid() ? "" : " (not set)");
        } else if (buf[0]) {
            Serial.println("commands: t <unix seconds> | s | games | games new | game <id> | ack <id> | bank | "
                           "erase-all yes | audit | sleeptiming on|off | n (new game) | o (power off) | "
                           "mode competition|league|coach | page");
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
    sleepWhileTiming = prefs.getBool("sleeptim", true);
    myPosition = prefs.getUChar("mypos", 0) % 4;
    if (!bankStore.begin()) Serial.println("databank: no \"games\" partition, games won't be kept");
    bank.begin();
    // The mode: whatever the rocker is doing while the side button switches the device on.
    if (board::heldAtStart(board::kDown)) mode = GameMode::League;
    else if (board::heldAtStart(board::kUp)) mode = GameMode::Coach;
    else mode = GameMode::Competition;
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
    auditStart(esp_timer_get_time());  // first clock audit sample
    nextAuditAt = esp_timer_get_time() + kAuditEveryUs;
    Serial.printf("stoprock ready (%s, %s mode)\n", board::name(), modeName(mode));
}

void loop() {
    int64_t now = esp_timer_get_time();
    pollButtons(now);
    handleOutcome(timing.poll(now), now);
    sound.update(now);
    serialCommands();

    // between stones: a ghost-clearing full refresh every kStonesPerClean stones. If the next
    // stone has already started, it waits for that stone to end, which schedules it again.
    if (refreshAt >= 0 && now >= refreshAt) {
        refreshAt = -1;
        if (!timing.active()) {
            stonesSinceClean = 0;
            syncModel(true);
        }
    }

    // the switch-on tip goes after 5 s. If a stone is being timed, it goes with that stone's
    // next screen update instead, so the screen isn't redrawn mid-stone just for it.
    if (tipWaiting && !displayBusy) {  // on screen now (the switch-on refresh takes ~2 s)
        tipWaiting = false;
        tipUntil = now + kTipUs;
    }
    if (tipUntil >= 0 && now >= tipUntil) {
        tipUntil = -1;
        if (!timing.active()) syncModel();
    }

    if (now >= nextBatteryCheck) {
        nextBatteryCheck = now + kBatteryCheckUs;
        const int pct = board::batteryPercent();
        const bool low = pct >= 0 && (lowBattery ? pct < 20 : pct < 15);
        if (low != lowBattery) { lowBattery = low; if (!timing.active()) syncModel(); }
    }

    if (now - lastActivity >= kPowerOffUs) showHelpAndPowerOff();

    // clock audit: start a sample every 10 minutes, then step it each pass
    if (now >= nextAuditAt && !auditWaiting) {
        nextAuditAt = now + kAuditEveryUs;
        auditStart(now);
    }
    auditStep();

    // light sleep when nothing is going on
    bool anyDown = false;
    for (const ButtonState& s : buttons) anyDown |= s.down || s.pending || s.rawDown;
    const bool mayTime = !timing.active() || sleepWhileTiming;
    if (mayTime && !displayBusy && !anyDown && !sound.playing() && !auditWaiting && now >= sleepAt) {
        int64_t wake = lastActivity + kPowerOffUs;
        if (refreshAt >= 0 && refreshAt < wake) wake = refreshAt;
        if (tipUntil >= 0 && tipUntil < wake) wake = tipUntil;
        if (nextAuditAt < wake) wake = nextAuditAt;
        if (timing.active() && timing.deadline() + 1000 < wake) wake = timing.deadline() + 1000;
        const int64_t before = esp_timer_get_time();
        if (board::lightSleep(wake)) {
            now = esp_timer_get_time();
            sleptUs += now - before;
            // A timing button that woke us: its press time is the wake-up time.
            for (uint8_t b : {board::kTop, board::kSide}) {
                if (board::held(static_cast<board::Button>(b)) && !buttons[b].pending) {
                    lastEdge[b] = now;
                    buttons[b].pending = true;
                    buttons[b].pendingT = now;
                }
            }
            sleepAt = now + kAwakeAfterWakeUs;
            return;
        }
    }
    delay(2);
}
