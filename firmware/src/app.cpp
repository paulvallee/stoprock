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
#include "throw_order.h"
#include "timing.h"

using namespace stoprock;

namespace {

constexpr int64_t kSec = 1000000;
constexpr int64_t kConfirmUs = 15000;          // a timing press must still be held 15 ms after its edge
constexpr int64_t kEdgeGapUs = 30000;          // ignore further edges of the same button within 30 ms
constexpr int64_t kStableUs = 20000;           // rocker debounce
constexpr int64_t kUndoHoldUs = kSec;       // hold the rocker centre 1 s to undo (and 1 s for each more)
constexpr int64_t kUndoHintUs = kSec / 5;   // ... "Keep holding to undo" shows 0.2 s into it (and
                                            // "Keep holding to move time" 0.2 s into a hold that moves a time)
constexpr int64_t kPowerHoldUs = 3 * kSec;
constexpr int64_t kOffHintUs = kSec / 2;       // "Keep holding to turn off" from then (a timing click is quicker)
constexpr int64_t kDoublePressUs = 400000;     // two rocker presses within 0.4 s: a double press
constexpr int64_t kFocusHoldUs = kSec / 2;     // league: hold rocker down (up from your rocks) to move the focus
constexpr int64_t kMoveStepUs = 700000;        // every mode: holding rocker up (down: left, competition) moves the newest time a column this often
constexpr int64_t kAwakeAfterPressUs = 2 * kSec;    // stay awake this long after a rocker press
constexpr int64_t kAwakeAfterWakeUs = 40000;        // time to confirm the button that woke us
constexpr int64_t kAuditEveryUs = 10 * 60 * kSec;   // clock audit sample interval
// Switch off after 2 hours with no press: a game is about that long, and someone who times only a
// stone or two in the last end mustn't lose the game to it. Light sleep costs little more meanwhile.
constexpr int64_t kPowerOffUs = 2 * 60 * 60 * kSec;
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
constexpr int kMaxStones = 32;  // kept for the screen: at least a screenful of each player (and THEM)

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
uint8_t myPosition = 0;         // 0 lead .. 3 skip (double press of the rocker centre; kept)

// What differs between the modes, all in one place: everything else is the same code for all three.
struct ModeRules {
    uint8_t shotsPerEnd;  // rocks logged an end: 0 competition, 2 league (yours), 8 coach (the team's)
    bool focus;           // league, coach: the rocker works on the times or on the rocks (hold down / up)
    bool positions;       // you have a position (double click changes it); a coach has none (double click: page)
};
const ModeRules& rules() {
    static const ModeRules kRules[3] = {  // indexed by GameMode: League, Competition, Coach
        {2, true, true},
        {0, false, true},
        {8, true, false},
    };
    return kRules[static_cast<uint8_t>(mode)];
}
// Where you sit (seats.h): your position, or a coach's seat with a column for every position.
uint8_t seat() { return rules().positions ? myPosition : kCoachSeat; }
uint8_t columns() { return columnCount(seat()); }

// The times are in a column per player (seats.h): your teammates (a coach: all four), then THEM.
// The cursor is on the player about to throw: the throw order (ThrowOrder) puts it on whoever
// throws next, and the rocker moves it too (or moves a time afterwards).
uint8_t activeCol = 0;          // the column with the cursor
// League: what rocker up/down work on, the focus (bold border): the players' columns (up = next,
// down = previous) or your rocks (made/missed). Hold down 0.5 s for your rocks; logging a shot (or
// holding up 0.5 s) goes back to the columns. On the columns, holding up moves the newest time.
Focus focus = Focus::Players;
ThrowOrder order;               // where in the end the game is
bool manualCursor = false;      // the cursor was put where it is with the rocker: leave it there
int64_t stoneStart = -1;        // when the stone being timed started
int64_t gameStartUs = 0;        // when this game started (switch-on)
// A stone started with no second click for longer than a B2H can be (5.5 s): it can only be an
// H2H, so the stopwatch moves to the H2H box. h2hSwitchAt: when that happens for the stone under way.
int64_t h2hSwitchAt = -1;
uint8_t moveSteps = 0;          // columns the newest time has moved during this hold of rocker up
// League: when your throw is next, the focus moves to your rocks by itself this long after the
// stone before yours started (about 13 s after it's done, if it was timed), ready for your shot.
constexpr int64_t kOwnShotFocusUs = 30 * kSec;
int64_t rocksFocusAt = -1;      // ... then; -1 = not due
// Between stones the cursor follows the clock, moving on when enough time has gone by for another
// throw nobody timed (ThrowOrder::nextChange). The next check is then.
int64_t followAt = -1;
int64_t lastRockerPress = -kSec; // when the rocker centre was last pressed (for a double press)

TimingEngine timing;
bool h2hOnly() { return h2hSwitchAt < 0 && stoneStart >= 0 && timing.state() == TimingState::Started; }
ShotLog shots;
Stone stones[kMaxStones];
uint8_t stoneCount = 0;

// Every entry of this game in the order it was made, so undo takes back whichever came last.
enum class Entry : uint8_t { Shot, Stone };
constexpr int kMaxEntries = 64 + GameRecord::kMaxStones;  // a coach's 64 rocks, 128 stones
Entry entries[kMaxEntries];
int entryCount = 0;
void addEntry(Entry e) {
    if (entryCount == kMaxEntries) {  // can't happen in a game; keep the newest if it does
        memmove(&entries[0], &entries[1], sizeof(Entry) * (kMaxEntries - 1));
        --entryCount;
    }
    entries[entryCount++] = e;
}


int64_t lastActivity = 0;
int64_t sleepAt = 0;
int64_t refreshAt = -1;          // after every 4th stone: a ghost-clearing full refresh
constexpr int64_t kTipUs = 5 * kSec;
int64_t tipUntil = -1;           // the switch-on tip shows until then
bool tipWaiting = false;         // ... its 5 s start once the screen showing it is up
bool undoHint = false;           // "Keep holding to undo": the rocker centre is held, the undo not yet due
bool offHint = false;            // "Keep holding to turn off": the side button held, not yet 3 s
bool moveHint = false;           // "Keep holding to move time": rocker up (or down) held to move a time, till let go
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
void restartOrder();
void newGame() {
    bank.startGame(board::clockNow(), board::clockValid(), board::batteryPercent(), mode);
    const uint8_t perEnd = rules().shotsPerEnd > 2 ? rules().shotsPerEnd : 2;  // competition logs none
    shots.setCapacity(8 * perEnd);  // 8 ends
    focus = Focus::Players;
    stoneStart = -1;
    gameStartUs = esp_timer_get_time();
    entryCount = 0;
    restartOrder();
    // the switch-on tip for 5 s: your position and the holds (coach: page switch and moving times)
    tipWaiting = true;
    tipUntil = -1;
    stoneCount = 0;
    Serial.printf("game: new, %s mode\n", modeName(mode));
}

// Seconds since this switch-on (gameStartUs), for the databank's stone and shot times.
uint16_t secondsIn(int64_t us) {
    if (us < 0 || us < gameStartUs) return StoneTimes::kNoTime;
    const int64_t s = (us - gameStartUs) / kSec;
    return s >= StoneTimes::kNoTime ? StoneTimes::kNoTime - 1 : static_cast<uint16_t>(s);
}

// shotAt: when a rock just logged was logged (secondsIn), kept with it in the databank.
void saveShots(uint16_t shotAt = StoneTimes::kNoTime) {
    bank.setShots(shots.count(), shots.bits(), board::clockNow(), shotAt);
}

uint16_t toStored(int32_t hundredths) {
    return hundredths < 0 || hundredths >= StoneTimes::kNoTime ? StoneTimes::kNoTime : static_cast<uint16_t>(hundredths);
}

// ------------------------------------------------------------------ display task
struct Model {
    GameMode mode = GameMode::League;
    uint8_t page = 0;       // coach: which page
    uint8_t me = 0;         // competition: your position
    uint8_t activeCol = 0;  // competition and league: the column with the cursor
    Stone stones[kMaxStones];
    uint8_t stoneCount = 0;
    ShotLog shots;          // the shots on screen (deferred while a stone is timed)
    bool warning = false;
    bool help = false;
    bool full = false;      // next update is a ghost-clearing full refresh
    Tip tip = Tip::None;    // a box over the times: the switch-on tip, or "Keep holding to undo"
    Waiting waiting = Waiting::None;  // a stone being timed: the stopwatch shows where its time will go
    Focus focus = Focus::Players;     // league: what the rocker works on
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
            if (m.page == 0)
                drawCoachTimer(next, m.stones, m.stoneCount, m.shots, m.waiting, m.activeCol, m.tip, m.focus);
            else drawCoachRocks(next, m.stones, m.stoneCount, m.shots, m.waiting, m.tip, m.focus);
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
        m.tip = offHint ? Tip::PowerOff : undoHint ? Tip::Undo : moveHint ? Tip::Move : tipWaiting || tipUntil >= 0 ? Tip::SwitchOn : Tip::None;
        m.focus = focus;
        m.waiting = timing.state() == TimingState::Started     ? (h2hOnly() ? Waiting::HogToHogOnly : Waiting::BackToHog)
                    : timing.state() == TimingState::WaitingHH ? Waiting::HogToHog
                                                               : Waiting::None;
    });
}

// ------------------------------------------------------------------ timing
// A new stone, newest first, filed under the player in the active column (see addStone()).
// stoneGuess: who the throw order guessed for it, whatever the cursor said (kept in the databank,
// to check the guesses against real games).
uint8_t stoneGuess = Stone::kNoPlayer;
void pushStone(Stone s) {
    s.player = columnPlayer(seat(), activeCol);
    stoneGuess = columnPlayer(seat(), order.predict(stoneStart));
    addStone(stones, stoneCount, kMaxStones, s);
    addEntry(Entry::Stone);
    order.place(activeCol, stoneStart);
    manualCursor = false;
}

// The cursor on whoever the throw order says throws next (unless you put it somewhere).
void cursorToNext(int64_t start = -1) {
    if (manualCursor) return;
    activeCol = order.predict(start);
}

// The next time the cursor may move on by itself, from now.
void planFollow(int64_t now) { followAt = order.nextChange(now); }

// The throw order moved on between stones: the cursor on whoever's next, and when it may move on.
void reaim(int64_t now) {
    if (timing.active()) return;  // a stone under way keeps its column; it re-aims when it's done
    cursorToNext();
    planFollow(now);
}

// A new game, or a new position: the throw order from the start of a game.
void restartOrder() {
    order.reset(seat());
    manualCursor = false;
    rocksFocusAt = -1;
    followAt = -1;
    cursorToNext();
}

// League: if your throw is next, or after one more (nobody timing the one between), the focus
// moves to your rocks once the stone before yours should be done.
void planRocksFocus() {
    rocksFocusAt = -1;
    if (!rules().focus || order.lastStart() < 0) return;
    const int k = order.throwsToMine();
    if (k == 1 || k == 2) rocksFocusAt = order.expectedStart(k - 1) + kOwnShotFocusUs;
}

void closeStone(int64_t now) {
    board::led(false);
    sleepAt = now;
}

// A timed stone finished (with an H2H, or a B2H only). Every kStonesPerClean-th stone gets a
// ghost-clearing full refresh at the safe moment, delay after it closed.
void countTimedStone(int64_t now, int64_t delay) {
    // The cursor goes on whoever throws next.
    cursorToNext();
    planFollow(now);
    Serial.printf("timing: next up, column %d\n", activeCol + 1);
    // League: the rocker goes to the times, or to your rocks if you threw since your last logged shot
    // (one owed: more means you aren't logging them, so don't pull the focus down every stone).
    if (rules().focus) {
        const Focus f = order.toLogSoFar() - shots.count() == 1 ? Focus::Rocks : Focus::Players;
        if (f != focus) Serial.printf("focus: %s (after a timed stone)\n", f == Focus::Rocks ? "rocks, a shot to log" : "players");
        focus = f;
        planRocksFocus();
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
        stoneStart = now;
        h2hSwitchAt = now + timing.config().bhMaxUs;
        rocksFocusAt = -1;  // someone's throwing: not you
        followAt = -1;      // the cursor stays on this stone's column until it's done
        cursorToNext(now);  // the time since the last stone says how many throws went untimed
        if (rules().focus && focus == Focus::Rocks) {  // timing: the rocker back on the times
            focus = Focus::Players;
            Serial.println("focus: players (timing)");
        }
        // no LED: the case covers it, and the stopwatch icon shows timing in progress
        Serial.println("timing: start");
        syncModel();  // a stopwatch where the time will appear
        break;
    case TimingEvent::Restarted:
        sleptAtAnchor = sleptUs;
        stoneStart = now;
        h2hSwitchAt = now + timing.config().bhMaxUs;
        cursorToNext(now);
        Serial.println("timing: double press, restart");
        syncModel();  // back to awaiting the B2H, if it had moved on
        break;
    case TimingEvent::BH:
        noteResult(o.us);
        pushStone(Stone{toHundredths(o.us), -1});
        bank.addStone(toStored(stones[0].bh), StoneTimes::kNoTime, board::clockNow(), stones[0].player, stoneGuess,
                      secondsIn(stoneStart));
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
            bank.addStone(StoneTimes::kNoTime, toStored(stones[0].hh), board::clockNow(), stones[0].player,
                          stoneGuess, secondsIn(stoneStart));
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
    addEntry(Entry::Shot);
    if (made) sound.play(kMade); else sound.play(kMissed);
    Serial.printf("shots: %s, %d/%d\n", made ? "made" : "missed", shots.madeCount(), shots.count());
    // Each logged rock pins the throw order: which of the team's eight stones of which end it was
    // (league: one of your two; coach: the team's, in throwing order).
    const int64_t now = esp_timer_get_time();
    const int i = shots.count() - 1, perEnd = rules().shotsPerEnd;
    const int k = perEnd == 8 ? i % 8 : 2 * myPosition + i % 2;
    order.teamShot(i / perEnd, k, now);
    manualCursor = false;
    reaim(now);
    if (rules().focus) {  // league: back to the columns, and your next rock planned
        focus = Focus::Players;
        planRocksFocus();
    }
    saveShots(secondsIn(now));
    if (!timing.active()) syncModel();
}

// Hold the rocker centre: undo the newest entry, a time or a shot, whichever came last (again every
// 0.5 s while held). A stone being timed is cancelled first: if it has its B2H, that time goes too.
// The throw order steps back with it. Returns false when there was nothing to undo.
bool onUndo() {
    const int64_t now = esp_timer_get_time();
    undoHint = false;  // the screen update below clears it
    if (timing.active()) {
        // its B2H is an entry already: undone below, if nothing (a coach's shot) was logged since
        const bool newestIsIt = timing.state() == TimingState::WaitingHH && entryCount > 0 &&
                                entries[entryCount - 1] == Entry::Stone;
        timing.reset();
        h2hSwitchAt = -1;
        closeStone(now);
        Serial.println("timing: cancelled");
        if (!newestIsIt) {
            sound.play(kUndo);
            reaim(now);
            syncModel();
            return true;
        }
    }
    if (entryCount == 0) {
        sound.play(kError);
        syncModel();  // clears "Keep holding to undo"
        return false;
    }
    const Entry e = entries[--entryCount];
    if (e == Entry::Shot) {
        shots.undo();
        saveShots();
        Serial.printf("undo: shot, %d/%d\n", shots.madeCount(), shots.count());
    } else {
        if (stoneCount > 0) {  // the newest stone is at the front
            memmove(&stones[0], &stones[1], sizeof(Stone) * (stoneCount - 1));
            --stoneCount;
        }
        bank.removeLastStone(board::clockNow());
        Serial.println("undo: time");
    }
    order.undo();
    manualCursor = false;
    reaim(now);
    if (rules().focus) planRocksFocus();
    sound.play(kUndo);
    syncModel();
    return true;
}

// A second short rocker press within 0.4 s (not an undo): changes your position (lead, 2nd, 3rd,
// skip, round again), or a coach's page. The first press does nothing by itself, so a double
// click never costs an entry.
void onRockerPress(ButtonState& s, int64_t t) {
    if (t - lastRockerPress < kDoublePressUs) {
        s.holdFired = true;      // this press can't turn into an undo
        lastRockerPress = -kSec; // a third press starts over
        sound.play(kClick);
        if (!rules().positions) {
            coachPage ^= 1;
            Serial.printf("coach: page %d\n", coachPage + 1);
        } else {
            myPosition = (myPosition + 1) % 4;
            prefs.putUChar("mypos", myPosition);
            restartOrder();
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
    activeCol = (activeCol + (dir < 0 ? columns() - 1 : 1)) % columns();
    manualCursor = true;
    rocksFocusAt = -1;  // you're on the columns on purpose
    sound.play(kClick);
    const uint8_t p = columnPlayer(seat(), activeCol);
    Serial.printf("timing: %s next\n", p == kThem ? "THEM" : kPositionNames[p]);
    syncModel();
}

// Rocker up held on the columns: the newest time moves a column right every 0.7 s (round and
// round), shown where it is; rocker down held (where it doesn't move the focus: competition, coach)
// moves it left. Returns false when there's no time to move.
bool stepMove(int dir) {
    const int col = stoneCount > 0 ? playerColumn(seat(), stones[0].player) : -1;
    if (col < 0) { sound.play(kError); return false; }
    const uint8_t to = static_cast<uint8_t>((col + (dir < 0 ? columns() - 1 : 1)) % columns());  // as a press moves the cursor
    stones[0].player = columnPlayer(seat(), to);
    activeCol = to;
    ++moveSteps;
    sound.play(kClick);
    syncModel();
    return true;
}

// ... and let go: it stays in that column, and the throw order is put right to match.
void finishMove() {
    const uint8_t col = static_cast<uint8_t>(playerColumn(seat(), stones[0].player));
    bank.setLastPlayer(stones[0].player, board::clockNow());
    order.move(col);
    manualCursor = false;
    if (timing.state() == TimingState::Started) {
        cursorToNext(stoneStart);  // a new stone under way: the cursor is where its time will go
    } else {
        // The cursor stays on the column the time was let go in (it's still being timed, or you just
        // put it there), not the next thrower: jumping on looked like the time kept moving. It
        // follows the clock again, and goes to the next thrower, when the next stone starts.
        activeCol = col;
        followAt = -1;
        if (!timing.active()) planRocksFocus();
    }
    Serial.printf("timing: newest time moved to column %d\n", col + 1);
    syncModel();
}

// League: rocker down held 0.5 s moves the focus to your rocks, rocker up held 0.5 s back to the columns.
void setFocus(Focus f) {
    focus = f;
    rocksFocusAt = -1;
    sound.play(kClick);
    Serial.printf("focus: %s\n", f == Focus::Players ? "players" : "rocks");
    syncModel();
}

// A short press of rocker up/down, acted on when it's let go (a long press moves the focus or the
// newest time instead): made/missed (coach, league on your rocks), or next/previous player.
void releaseAction(uint8_t b);
void onRelease(uint8_t b) {
    const bool hadHint = moveHint && (b == board::kUp || b == board::kDown);
    if (hadHint) moveHint = false;  // let go: "Keep holding to move time" goes
    releaseAction(b);
    if (hadHint) syncModel();  // (an update the action made already took it; this one changes nothing then)
}

void releaseAction(uint8_t b) {
    if (b == board::kPress && undoHint) {  // let go before the undo: nothing undone
        undoHint = false;
        syncModel();
    }
    if (b != board::kUp && b != board::kDown) return;
    if (moveSteps > 0) { finishMove(); return; }  // a held up (or down) moved the newest time
    if (buttons[b].holdFired) return;
    if (rules().shotsPerEnd && (!rules().focus || focus == Focus::Rocks)) onShot(b == board::kUp);
    else movePlayer(b == board::kUp ? 1 : -1);  // up = next player, down = previous
}

void onPress(uint8_t b, int64_t t) {
    ButtonState& s = buttons[b];
    s.down = true;
    s.downSince = t;
    s.holdFired = false;
    s.nextRepeat = t + kUndoHoldUs;
    if (b == board::kUp || b == board::kDown) moveSteps = 0;
    activity(t);
    // Rocker up/down act when let go (next/previous player, or made/missed), or move the focus or
    // the newest time when held (pollButtons).
    if (b == board::kUp || b == board::kDown) return;
    switch (b) {
    case board::kTop:
    case board::kSide: onTimingPress(t); break;
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
            if (s.down && !raw && stable) {
                s.down = false;
                if (b == board::kSide && offHint) {  // let go before 3 s: stays on
                    offHint = false;
                    syncModel();
                }
            }
        } else {
            if (stable && raw && !s.down) onPress(b, s.rawSince);
            if (stable && !raw && s.down) {
                onRelease(b);
                s.down = false;
            }
        }

        // holds
        if (s.down && !s.holdFired) {
            // Rocker press: undo the newest entry after 1 s, then again every 1 s while held.
            // "Keep holding to undo" shows from 0.2 s until the first undo only: shown again for the
            // repeats, it would vanish and reappear at every one. Stops after one error buzz once
            // there's nothing left to undo.
            if (b == board::kPress && !undoHint && now < s.downSince + kUndoHoldUs &&
                now >= s.downSince + kUndoHintUs) {
                undoHint = true;
                syncModel();
            }
            if (b == board::kPress && now >= s.nextRepeat) {
                s.nextRepeat += kUndoHoldUs;
                lastRockerPress = -kSec;  // a hold isn't half of a double press
                activity(now);
                if (!onUndo()) s.holdFired = true;
            }
            if (b == board::kSide && !offHint && now - s.downSince >= kOffHintUs) {
                offHint = true;
                syncModel();
            }
            if (b == board::kSide && now - s.downSince >= kPowerHoldUs) {
                s.holdFired = true;
                offHint = false;
                showHelpAndPowerOff();
            }
            // League: hold down for your rocks; hold up for the columns. Every mode: hold up on the
            // columns to move the newest time a column right every 0.7 s (let go where it belongs);
            // without a focus (competition, coach), hold down to move it left.
            const bool rocker = b == board::kUp || b == board::kDown;
            const bool moves = rocker && raw && (b == board::kUp || !rules().focus) && focus == Focus::Players;
            if (moves && !moveHint && stoneCount > 0 && now - s.downSince >= kUndoHintUs) {
                moveHint = true;  // a press of up/down is quicker: it never shows for one
                syncModel();
            }
            if (rules().focus && rocker && (b == board::kDown || focus == Focus::Rocks) &&
                now - s.downSince >= kFocusHoldUs) {
                s.holdFired = true;  // not a short press when it's let go
                activity(now);
                setFocus(b == board::kUp ? Focus::Players : Focus::Rocks);
            } else if (rocker && raw && (b == board::kUp || !rules().focus) && focus == Focus::Players &&
                       now - s.downSince >= kMoveStepUs * (moveSteps + 1)) {
                // raw: no step once it's let go (it counts as released only after 20 ms up)
                activity(now);  // up moves the newest time right; down (no focus to move) left
                if (!stepMove(b == board::kUp ? 1 : -1)) s.holdFired = true;
            }
        }
    }
}

// Databank export, framed so an app can parse it: "@GAMES n", n JSON lines, "@END".
void printGames(uint32_t fromId) {
    static GameRecord g;
    static char json[6400];  // a full coach game: 64 shots and their times, 128 stones
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

// One console command (USB serial, or the simulator's second port: board::command()).
void runCommand(const char* buf) {
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
        else { Serial.println("modes: competition | league | coach"); return; }
        coachPage = 0;
        newGame();
        syncModel(true);
    } else if (!strcmp(buf, "page")) {  // testing: coach page switch, as a double press
        if (!rules().positions) { coachPage ^= 1; syncModel(); }
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

void serialCommands() {
    static char buf[48];
    static uint8_t len = 0;
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c != '\n' && c != '\r') { if (len < sizeof buf - 1) buf[len++] = c; continue; }
        buf[len] = 0;
        len = 0;
        runCommand(buf);
    }
    static char other[48];
    if (board::command(other, sizeof other)) runCommand(other);
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
    if (h2hSwitchAt >= 0 && now >= h2hSwitchAt) {  // no B2H in time: it's an H2H under way
        h2hSwitchAt = -1;
        if (timing.state() == TimingState::Started) {
            Serial.println("timing: no B2H, an H2H under way");
            syncModel();
        }
    }
    if (followAt >= 0 && now >= followAt) {  // another throw's worth of time: the cursor may move on
        planFollow(now);
        if (!timing.active() && !manualCursor) {
            const uint8_t col = order.predict(now);
            if (col != activeCol) {
                activeCol = col;
                Serial.printf("timing: next up by the clock, column %d\n", activeCol + 1);
                syncModel();
            }
        }
    }
    if (rocksFocusAt >= 0 && now >= rocksFocusAt) {  // your throw: ready to log it
        rocksFocusAt = -1;
        if (rules().focus && !timing.active() && focus == Focus::Players) {
            focus = Focus::Rocks;
            Serial.println("focus: rocks (your throw)");
            syncModel();
        }
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
        if (rocksFocusAt >= 0 && rocksFocusAt < wake) wake = rocksFocusAt;
        if (followAt >= 0 && followAt < wake) wake = followAt;
        if (h2hSwitchAt >= 0 && h2hSwitchAt < wake) wake = h2hSwitchAt;
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
