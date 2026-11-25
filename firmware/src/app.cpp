// stoprock application: buttons, timing, shots, games, sounds, sleep and the display task.
// Behaviour follows docs/SPEC.md; the hardware is behind board.h.
#include <Arduino.h>
#include <Preferences.h>
#include <esp_core_dump.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <time.h>

#include "board.h"
#include "databank.h"
#include "screens.h"
#include "shots.h"
#include "startup_menu.h"
#include "throw_order.h"
#include "timing.h"
#include "tutorials.h"

using namespace stoprock;

namespace {

constexpr int64_t kSec = 1000000;
constexpr int64_t kConfirmUs = 15000;          // a timing press must still be held 15 ms after its edge
constexpr int64_t kEdgeGapUs = 30000;          // ignore further edges of the same button within 30 ms
constexpr int64_t kStableUs = 20000;           // rocker debounce
constexpr int64_t kUndoHoldUs = kSec;       // hold the rocker centre 1s to undo (and 1s for each more)
constexpr int64_t kUndoHintUs = kSec / 5;   // ... "Keep holding to undo" shows 0.2s into it
constexpr int64_t kMoveHintUs = 350000;       // "Moving time" 0.35s into a hold of up on the columns: a press to
                                               // move the cursor can last 0.2s; the move itself is at 1s
constexpr int64_t kPowerHoldUs = 3 * kSec;
constexpr int64_t kOffHintUs = kSec / 2;       // "Keep holding to turn off" from then (a timing click is quicker)
constexpr int64_t kDoublePressUs = 400000;     // two rocker presses within 0.4s: a double press
constexpr int64_t kFocusHoldUs = kSec / 2;     // league: hold rocker down (up from your rocks) to move the focus
constexpr int64_t kMoveStepUs = kSec;          // holding up moves the newest time this often (first at 1s)
constexpr int64_t kScrollModeHoldUs = 700000;  // hold down to browse or log rocks, hold up to return
constexpr int64_t kAwakeAfterPressUs = 2 * kSec;    // stay awake this long after a rocker press
constexpr int64_t kAwakeAfterWakeUs = 40000;        // time to confirm the button that woke us
constexpr int64_t kAuditEveryUs = 10 * 60 * kSec;   // clock audit sample interval
// Switch off after 2 hours with no press: a game is about that long, and someone who times only a
// stone or two in the last end mustn't lose the game to it. Light sleep costs little more meanwhile.
constexpr int64_t kPowerOffUs = 2 * 60 * 60 * kSec;
constexpr int64_t kCleanAfterHhUs = 15 * kSec;  // the ghost-clean waits this long after an H2H: time to read it
constexpr uint8_t kStonesPerClean = 4;         // ghost-clearing full refresh after every 4th timed stone
#ifdef STOPROCK_BOARD_EMU
// Simulator: react to the battery slider quickly.
constexpr int64_t kBatteryCheckUs = 2 * kSec;
constexpr uint32_t kMinGameSecondsForBattery = 60;  // allow short games when testing the slider
#else
constexpr int64_t kBatteryCheckUs = 30 * kSec;
constexpr uint32_t kMinGameSecondsForBattery = 20 * 60;
#endif
constexpr int kMaxStones = 32;  // kept for the screen: at least a screenful of each player (and THEM)

const char* const kPositionNames[4] = {"lead", "second", "third", "skip"};

// ------------------------------------------------------------------ sounds
struct Note { uint16_t hz, ms; uint8_t volume = 67; };
const Note kClick[] = {{2400, 30}};
const Note kMade[] = {{3000, 70}};
const Note kMissed[] = {{1000, 110}};
const Note kUndo[] = {{1800, 60}, {0, 25}, {1200, 80}};
constexpr Note kError[] = {{300, 180, 50}};
constexpr Note kBasicError[] = {{kError[0].hz, kError[0].ms, 5}};

// The Sounds page's settings (kept as "sounds"): muted, and each kind's level.
SoundSettings soundSettings;

struct Sound {
    const Note* notes = nullptr;
    uint8_t count = 0, index = 0;
    SoundKind kind = SoundKind::Click;
    bool preview = false;
    int64_t until = 0;

    // A sound of a kind, at its level (nothing when that's off, or muted). preview: the Sounds page
    // trying a level, heard even while muted.
    template <size_t N> void play(const Note (&n)[N], SoundKind k, bool tryLevel = false) {
        if (soundVolume(soundSettings, k, 100, tryLevel) == 0) {  // silent: stop whatever was playing
            if (notes) board::tone(0, 0);
            notes = nullptr;
            return;
        }
        notes = n;
        count = N;
        index = 0;
        kind = k;
        preview = tryLevel;
        tone(n[0]);
        until = esp_timer_get_time() + n[0].ms * 1000LL;
    }
    bool playing() const { return notes != nullptr; }
    void update(int64_t now) {
        if (!notes || now < until) return;
        if (++index >= count) { notes = nullptr; board::tone(0, 0); return; }
        tone(notes[index]);
        until = now + notes[index].ms * 1000LL;
    }
    void tone(const Note& n) const { board::tone(n.hz, soundVolume(soundSettings, kind, n.volume, preview)); }
} sound;

void loadSoundSettings(Preferences& p) {
    SoundSettings s;
    if (p.getBytesLength("sounds") == sizeof s) p.getBytes("sounds", &s, sizeof s);
    s.sanitize();
    soundSettings = s;
}

// ------------------------------------------------------------------ buttons
struct ButtonState {
    bool down = false;
    bool rawDown = false;
    int64_t rawSince = 0;
    int64_t downSince = 0;
    bool holdFired = false;
    int64_t nextRepeat = 0;   // when a held rocker repeats an undo
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

void playError() { sound.play(mode == GameMode::Basic ? kBasicError : kError, SoundKind::Error); }

// What differs between the modes, all in one place: everything else is the same code for all four.
struct ModeRules {
    uint8_t shotsPerEnd;  // rocks logged an end: 0 competition, 2 league (yours), 8 coach (the team's)
    bool focus;           // league, coach: the rocker works on the times or on the rocks (hold down / up)
    bool autoFocus;       // league: the focus goes to your rocks when you owe a shot, back after you log
                          // it; a coach (who may log many rocks at once) keeps it where they put it.
                          // Either way, starting the stopwatch brings it to the times.
    bool positions;       // you have a position (double click changes it); a coach has none (double click: page)
    bool columns;         // the times are in a column per player, with a cursor (basic: no players, a row a stone)
    bool pages;           // coach: two pages (double click switches)
};
const ModeRules& rules() {
    static const ModeRules kRules[4] = {  // indexed by GameMode: League, Competition, Coach, Basic
        {2, true, true, true, true, false},
        {0, false, false, true, true, false},
        {8, true, false, false, true, true},
        {0, false, false, false, false, false},
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
TimeHistoryView timeHistory;    // shared by the three modes that have player columns
// The timeline (TimeHistoryView, every mode with player columns): THEM between the teammate before
// (left) and the teammate whose pair is under way or next (right); the rocker steps round the cycle
// (timelineCycle()). timelineSide: which way the window slid to show a column beyond them, when you
// moved there (0: back on the three, or the clock put it there).
uint8_t timelineRight = 1;
int8_t timelineSide = 0;
int basicScroll = 0;             // basic: rows scrolled down, to older stones (the rocker; a new stone resets it)
// League: what rocker up/down work on, the focus (bold border): the players' columns (up = next,
// down = previous) or your rocks (made/missed). Hold down 0.5s for your rocks; logging a shot (or
// holding up 0.5s) goes back to the columns. On the columns, holding up moves the newest time.
Focus focus = Focus::Players;
ThrowOrder order;               // where in the end the game is
bool manualCursor = false;      // the cursor was put where it is with the rocker: leave it there
int64_t stoneStart = -1;        // when the stone being timed started
int64_t gameStartUs = 0;        // when this game started (switch-on)
// A stone started with no second click for longer than a B2H can be (7s): it can only be an
// H2H, so the stopwatch moves to the H2H box. h2hSwitchAt: when that happens for the stone under way.
int64_t h2hSwitchAt = -1;
uint8_t moveSteps = 0;          // columns the newest time has moved during this hold of rocker up
// League: when your throw is next, the focus moves to your rocks by itself this long after the
// stone before yours started (about 13s after it's done, if it was timed), ready for your shot.
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
constexpr int64_t kBasicIntroUs = 20 * kSec;
int64_t tipUntil = -1;           // the switch-on tip shows until then
bool tipWaiting = false;         // start the timer when the tip is actually on screen
bool basicIntroClearing = false; // presses still dismiss only while the display clears it
bool undoHint = false;           // "Keep holding to undo": the rocker centre is held, the undo not yet due
// Coach: a single click of the rocker centre (switching page takes two): "Double click rocker middle
// to change pages", once the double-click window has passed, for 3s or till the next press.
constexpr int64_t kPageTipUs = 3 * kSec;
int64_t singleClickAt = -1;     // the click to check, once the window has passed (-1: none)
int64_t singleClickPress = -1;  // ... the press it was (a second press since: a double click)
int64_t pageTipUntil = -1;
// The position card and a press of the rocker centre: dismissed once the double-click window has
// passed (a double click changes position and shows the card again: no blink in between).
int64_t cardDismissAt = -1;
bool offHint = false;            // "Keep holding to turn off": the side button held, not yet 3s
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

// ------------------------------------------------------------------ crashes
// What a crash left behind, so the next export says why. On battery a crash looks like switching off
// (the restart lets go of the power latch, and the next start is a plain power-on), so the reset
// reason rarely tells: the core dump the panic handler writes to flash (the "coredump" partition)
// does, read here at the next start. Brownouts and watchdog resets that do restart (on USB) are
// counted too. Printed by the `crash` command and with every games listing ("@CRASH").
char crashNote[360] = "";  // the last crash: when, the task, where, and the backtrace (decode with the build's ELF)

const char* resetName(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON: return "power-on";
        case ESP_RST_SW: return "restart";
        case ESP_RST_PANIC: return "crash (panic)";
        case ESP_RST_INT_WDT: return "crash (interrupt watchdog)";
        case ESP_RST_TASK_WDT: return "crash (task watchdog)";
        case ESP_RST_WDT: return "crash (watchdog)";
        case ESP_RST_BROWNOUT: return "brownout (supply too low)";
        case ESP_RST_DEEPSLEEP: return "deep sleep";
        case ESP_RST_EXT: return "reset pin";
        default: return "unknown";
    }
}

void noteCrashes() {
    const esp_reset_reason_t r = esp_reset_reason();
    Serial.printf("start: %s\n", resetName(r));
    if (r != ESP_RST_POWERON && r != ESP_RST_SW && r != ESP_RST_DEEPSLEEP && r != ESP_RST_EXT) {
        prefs.putUShort("resets", prefs.getUShort("resets", 0) + 1);
        prefs.putUChar("lastreset", static_cast<uint8_t>(r));
    }
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
    size_t addr = 0, size = 0;
    if (esp_core_dump_image_get(&addr, &size) == ESP_OK) {
        static esp_core_dump_summary_t s;
        if (esp_core_dump_get_summary(&s) == ESP_OK) {
            int n = snprintf(crashNote, sizeof crashNote, "seen at clock %lu, task %s, pc 0x%08lx, cause %lu, addr 0x%08lx, elf %.8s, bt",
                             static_cast<unsigned long>(board::clockNow()), s.exc_task,
                             static_cast<unsigned long>(s.exc_pc), static_cast<unsigned long>(s.ex_info.exc_cause),
                             static_cast<unsigned long>(s.ex_info.exc_vaddr), reinterpret_cast<const char*>(s.app_elf_sha256));
            for (uint32_t i = 0; i < s.exc_bt_info.depth && i < 16 && n < static_cast<int>(sizeof crashNote) - 12; ++i)
                n += snprintf(crashNote + n, sizeof crashNote - n, " 0x%08lx", static_cast<unsigned long>(s.exc_bt_info.bt[i]));
            if (s.exc_bt_info.corrupted && n < static_cast<int>(sizeof crashNote) - 12)
                snprintf(crashNote + n, sizeof crashNote - n, " (corrupted)");
            prefs.putString("crash", crashNote);
            prefs.putUShort("crashes", prefs.getUShort("crashes", 0) + 1);
            Serial.printf("start: the last run crashed: %s\n", crashNote);
        }
        esp_core_dump_image_erase();  // read once: the next crash writes a new one
    }
#endif
    if (!crashNote[0]) prefs.getString("crash", crashNote, sizeof crashNote);
}

void printCrashes() {
    const uint16_t resets = prefs.getUShort("resets", 0);
    Serial.printf("@CRASH {\"crashes\":%u,\"resets\":%u,\"last_reset\":\"%s\",\"last\":\"%s\"}\n",
                  prefs.getUShort("crashes", 0), resets,
                  resets ? resetName(static_cast<esp_reset_reason_t>(prefs.getUChar("lastreset", 0))) : "",
                  crashNote);
}

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
    timeHistory = {};
    lastRockerPress = -kSec;
    singleClickAt = -1;
    restartOrder();
    // Basic shows the tutorial; position modes show the switch-on tip.
    tipWaiting = mode == GameMode::Basic || rules().positions;
    basicIntroClearing = false;
    tipUntil = -1;
    cardDismissAt = -1;
    stoneCount = 0;
    basicScroll = 0;
    // Switching on draws with the fast waveform (no mode switch before the first click), which can
    // leave the wake screen or menu ghosting: the first ghost-clearing refresh comes after the first
    // stone, between stones, then every kStonesPerClean as usual.
    stonesSinceClean = kStonesPerClean - 1;
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
enum class MenuPage : uint8_t { Modes, Tutorials, Slide, Sounds };

struct Model {
    GameMode mode = GameMode::League;
    uint8_t page = 0;       // coach: which page
    uint8_t me = 0;         // competition: your position
    uint8_t activeCol = 0;  // competition and league: the column with the cursor
    TimeHistoryView history;
    int basicScroll = 0;
    Stone stones[kMaxStones];
    uint8_t stoneCount = 0;
    ShotLog shots;          // the shots on screen (deferred while a stone is timed)
    bool warning = false;
    bool off = false;
    int offBattery = -1;
    int offGamesLeft = -1;
    bool menu = false;      // the mode menu at switch-on
    MenuPage menuPage = MenuPage::Modes;
    StartupMenuSelection menuSelection;
    GameMode tutorialMode = GameMode::Basic;
    uint8_t tutorialPage = 0;
    SoundSettings sounds;   // the Sounds page
    board::Refresh refresh = board::Refresh::Partial;
    Tip tip = Tip::None;    // a box over the times: the switch-on tip, or "Keep holding to undo"
    Waiting waiting = Waiting::None;  // a stone being timed: the stopwatch shows where its time will go
    Focus focus = Focus::Players;     // league: what the rocker works on
};

Model model;
SemaphoreHandle_t modelLock;
TaskHandle_t displayTaskHandle;
// True from a model change until the screen shows it: the app doesn't sleep (or switch off) till then.
// Set and cleared under modelLock, with modelSeq telling the display task whether it has shown the
// latest (a change made while it was drawing the one before means another pass).
volatile bool displayBusy = false;
uint32_t modelSeq = 0;

void publish(void (*edit)(Model&)) {
    xSemaphoreTake(modelLock, portMAX_DELAY);
    edit(model);
    ++modelSeq;
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
        const uint32_t seq = modelSeq;
        model.refresh = board::Refresh::Partial;
        xSemaphoreGive(modelLock);

        if (m.off) {
            drawOffPage(next, m.offBattery, m.offGamesLeft);
        } else if (m.menu) {
            if (m.menuPage == MenuPage::Slide) drawTutorial(next, m.tutorialMode, m.tutorialPage);
            else if (m.menuPage == MenuPage::Sounds) drawSoundsMenu(next, m.sounds, m.menuSelection);
            else if (m.menuPage == MenuPage::Tutorials)
                drawStartupMenu(next, kTutorialMenuItems, kTutorialMenuCount, m.menuSelection, kTutorialsHeading);
            else drawStartupMenu(next, kStartupMenuItems, kStartupMenuCount, m.menuSelection);
        } else if (m.mode == GameMode::Basic) {
            drawBasic(next, m.stones, m.stoneCount, m.warning, m.tip, m.waiting, m.basicScroll);
        } else if (m.mode == GameMode::Competition) {
            drawCompetition(next, m.stones, m.stoneCount, m.me, m.activeCol, m.warning, m.tip,
                            m.waiting, m.history);
        } else if (m.mode == GameMode::Coach) {
            if (m.page == 0)
                drawCoachTimer(next, m.stones, m.stoneCount, m.shots, m.waiting, m.activeCol, m.tip, m.focus, m.history);
            else drawCoachRocks(next, m.stones, m.stoneCount, m.shots, m.waiting, m.tip, m.focus);
        } else {
            drawLeague(next, m.stones, m.stoneCount, m.shots, m.me, m.activeCol, m.warning, m.tip,
                       m.waiting, m.focus, m.history);
        }
        const int64_t t0 = esp_timer_get_time();
        const Rect update = m.refresh == board::Refresh::Partial ? next.diff(shown) : Rect{0, 0, Canvas::W, Canvas::H};
        if (!update.empty()) board::show(next, update, m.refresh);
        const char* refreshName = m.refresh == board::Refresh::FastScreen ? "fast screen" :
                                  m.refresh == board::Refresh::Full ? "full" : "partial";
        Serial.printf("display: %s %lld ms\n", refreshName, (esp_timer_get_time() - t0) / 1000);
        shown = next;
        xSemaphoreTake(modelLock, portMAX_DELAY);
        const bool more = modelSeq != seq;  // changed while drawing: another pass (publish() notified)
        if (!more) displayBusy = false;
        xSemaphoreGive(modelLock);
    }
}

// Copy the app's state into the screen model. Shots are held back while a stone is timed.
void syncModel(board::Refresh refresh = board::Refresh::Partial) {
    static board::Refresh requestedRefresh;
    requestedRefresh = refresh;
    if (rules().columns) {  // the timeline, from the throw order (by the clock, or the stone under way)
        const int64_t at = timing.active() && stoneStart >= 0 ? stoneStart : esp_timer_get_time();
        uint8_t left;
        order.besideThem(at, left, timelineRight);
        uint8_t cycle[kCoachColumns];
        timelineCycle(seat(), timelineRight, cycle);
        if (activeCol == cycle[0] || activeCol == cycle[1] || activeCol == cycle[2]) timelineSide = 0;
        timeHistory.timeline = true;
        timeHistory.hiddenSide = timelineColumns(seat(), timelineRight, activeCol, timelineSide, timeHistory.visible);
    }
    if (rules().columns) {
        timeHistory.first = firstVisibleTimeColumn(timeHistory.first, activeCol, columns());
        if (timeHistory.offset > 0) {
            uint8_t shown[kVisibleTimeColumns];
            historyColumns(timeHistory, activeCol, columns(), shown);
            const int limit = historyScrollLimit(stones, stoneCount, seat(), shown);
            if (timeHistory.offset > limit) timeHistory.offset = limit;
        }
    }
    {  // basic: never scrolled past the end (an undo can take a row away)
        const int most = stoneCount + (timing.state() == TimingState::Started ? 1 : 0) - 4;
        if (basicScroll > (most > 0 ? most : 0)) basicScroll = most > 0 ? most : 0;
    }
    publish([](Model& m) {
        m.mode = mode;
        m.page = coachPage;
        m.me = myPosition;
        m.activeCol = activeCol;
        m.history = timeHistory;
        m.basicScroll = basicScroll;
        memcpy(m.stones, stones, sizeof stones);
        m.stoneCount = stoneCount;
        if (!timing.active()) m.shots = shots;
        m.warning = lowBattery;
        m.off = false;
        m.menu = false;
        // Timing may update the queued frame before the display task takes it: keep the strongest
        // refresh asked for (Partial < FastScreen < Full), so a pending ghost-clean isn't lost.
        if (requestedRefresh > m.refresh) m.refresh = requestedRefresh;
        const Tip switchOn = mode == GameMode::Basic ? Tip::BasicIntro : Tip::SwitchOn;
        m.tip = offHint ? Tip::PowerOff : undoHint ? Tip::Undo : moveHint ? Tip::Move :
                pageTipUntil >= 0 ? Tip::Page : tipWaiting || tipUntil >= 0 ? switchOn : Tip::None;
        m.focus = timeHistory.scrolling ? Focus::Players : focus;
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
    s.player = rules().columns ? columnPlayer(seat(), activeCol) : Stone::kNoPlayer;
    stoneGuess = rules().columns ? columnPlayer(seat(), order.predict(stoneStart)) : Stone::kNoPlayer;
    addStone(stones, stoneCount, kMaxStones, s);
    addEntry(Entry::Stone);
    if (rules().columns) order.place(activeCol, stoneStart);
    manualCursor = false;
}

// The cursor on whoever the throw order says throws next (unless you put it somewhere).
void cursorToNext(int64_t start = -1) {
    if (manualCursor || timeHistory.scrolling || !rules().columns) return;
    activeCol = order.predict(start);
}

// Coach page 2 shows the rocks, not the columns: there the rocker always works on the rocks.
bool rocksPage() { return rules().pages && coachPage == 1; }

// The focus the device moves to by itself (timing, a stone done, a shot logged): never off the
// rocks on the rocks page.
void focusTo(Focus f) { focus = rocksPage() ? Focus::Rocks : f; }

// The next time the cursor may move on by itself, from now.
void planFollow(int64_t now) { followAt = rules().columns ? order.nextChange(now) : -1; }

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
    if (!rules().autoFocus || order.lastStart() < 0) return;
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
    if (rules().autoFocus) {
        const Focus f = order.toLogSoFar() - shots.count() == 1 ? Focus::Rocks : Focus::Players;
        if (f != focus) Serial.printf("focus: %s (after a timed stone)\n", f == Focus::Rocks ? "rocks, a shot to log" : "players");
        focusTo(f);
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
        basicScroll = 0;  // basic: back to the top, where the stone under way shows
        // tips go: timing has the screen (the hold hints stay while their hold lasts)
        tipWaiting = false;
        tipUntil = -1;
        pageTipUntil = -1;
        singleClickAt = -1;
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
// takes up to ~1s, so it runs a step per loop pass (buttons keep working meanwhile) and the
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
void showOffPageAndPowerOff() {
    static int battery, gamesLeft;
    battery = board::batteryPercent();
    bank.endGame(battery, board::clockNow());
    gamesLeft = bank.gamesLeft(battery, kMinGameSecondsForBattery);
    auditSampleNow();
    Serial.println("power: off");
    publish([](Model& m) {
        m.off = true;
        m.offBattery = battery;
        m.offGamesLeft = gamesLeft;
        m.refresh = board::Refresh::Full;
    });
    while (displayBusy) delay(10);
    board::led(false);
    board::tone(0, 0);
    board::powerOff();
    ESP.restart();  // only reached where power-off isn't possible (USB, simulator)
}

// ------------------------------------------------------------------ input actions
// ------------------------------------------------------------------ the mode menu
int menuIndex(GameMode mode) {
    int fallback = 0;
    for (int i = 0; i < kStartupMenuCount; ++i) {
        if (kStartupMenuItems[i].action != StartupMenuAction::Mode) continue;
        const GameMode itemMode = kStartupMenuItems[i].mode;
        if (itemMode == mode) return i;
        if (itemMode == GameMode::Competition) fallback = i;
    }
    return fallback;
}
constexpr int64_t kMenuIdleUs = 5 * 60 * kSec;  // untouched this long at the menu: switch off

void saveSoundSettings() {
    prefs.putBytes("sounds", &soundSettings, sizeof soundSettings);
    Serial.printf("sounds: %s, levels clicks %u made %u missed %u undo %u error %u\n",
                  soundSettings.muted ? "muted" : "on", soundSettings.level[0], soundSettings.level[1],
                  soundSettings.level[2], soundSettings.level[3], soundSettings.level[4]);
}

// A sound at its new level on the Sounds page (heard even while muted; nothing at off).
void playPreview(SoundKind k) {
    switch (k) {
        case SoundKind::Click: sound.play(kClick, k, true); break;
        case SoundKind::Made: sound.play(kMade, k, true); break;
        case SoundKind::Missed: sound.play(kMissed, k, true); break;
        case SoundKind::Undo: sound.play(kUndo, k, true); break;
        case SoundKind::Error: sound.play(kError, k, true); break;
    }
}

// At switch-on, unless the rocker is held: the modes, the one used last selected. Up/down
// choose (round and round), the rocker's middle starts the one selected. The side button held 3s
// switches off as always (once it's been let go after switching on).
GameMode chooseMode() {
    static StartupMenuSelection selection, tutorialSelection, soundSelection;
    static MenuPage page;
    static GameMode tutorialMode;
    static uint8_t tutorialPage;
    static bool full;
    // the mode itself is kept (not its place in the menu, which may change); competition the first time
    const uint8_t last = prefs.getUChar("menumode", static_cast<uint8_t>(GameMode::Competition));
    selection = {};
    selection.select(menuIndex(static_cast<GameMode>(last)), kStartupMenuCount);
    tutorialSelection = {};
    soundSelection = {};
    page = MenuPage::Modes;
    auto show = [](bool f) {
        full = f;
        publish([](Model& m) {
            m.menu = true;
            m.menuPage = page;
            m.menuSelection = page == MenuPage::Modes ? selection
                              : page == MenuPage::Sounds ? soundSelection : tutorialSelection;
            m.sounds = soundSettings;
            m.tutorialMode = tutorialMode;
            m.tutorialPage = tutorialPage;
            m.off = false;
            if (full && m.refresh < board::Refresh::FastScreen) m.refresh = board::Refresh::FastScreen;
        });
    };
    show(true);
    Serial.println("menu: choose a mode");
    const board::Button rocker[3] = {board::kUp, board::kPress, board::kDown};
    bool down[3] = {}, seen[3] = {};  // debounced state, and the previous sample
    int64_t lastInput = esp_timer_get_time(), sideSince = -1;
    bool sideLetGo = !board::held(board::kSide);
    for (;;) {
        const int64_t now = esp_timer_get_time();
        sound.update(now);
        for (int i = 0; i < 3; ++i) {
            const bool raw = board::held(rocker[i]);
            const bool pressed = raw && seen[i] && !down[i];  // two samples, 10 ms apart: not a bounce
            seen[i] = raw;
            if (raw == down[i] || (raw && !pressed)) continue;
            down[i] = raw;
            if (!raw) continue;
            lastInput = now;
            const int dir = rocker[i] == board::kUp ? -1 : 1;
            if (page == MenuPage::Sounds) {
                // All sounds: on / muted. A sound: its next level, played as a preview. Back: the modes.
                if (rocker[i] != board::kPress) {
                    sound.play(kClick, SoundKind::Click);
                    soundSelection.select(soundSelection.selected + dir, kSoundMenuCount);
                } else if (soundSelection.selected == 0) {
                    soundSettings.muted = !soundSettings.muted;
                    if (!soundSettings.muted) sound.play(kClick, SoundKind::Click);
                    saveSoundSettings();
                } else if (soundSelection.selected <= kSoundKinds) {
                    const auto k = static_cast<SoundKind>(soundSelection.selected - 1);
                    soundSettings.step(k);
                    playPreview(k);
                    saveSoundSettings();
                } else {
                    sound.play(kClick, SoundKind::Click);
                    page = MenuPage::Modes;
                }
                show(false);
                continue;
            }
            sound.play(kClick, SoundKind::Click);
            if (page == MenuPage::Slide) {
                if (rocker[i] == board::kPress) page = MenuPage::Tutorials;
                else {
                    const int next = tutorialPage + dir;
                    // past either end (the first card shows no Back, the last no Next): the list
                    if (next < 0 || next >= tutorialPageCount(tutorialMode)) page = MenuPage::Tutorials;
                    else tutorialPage = next;
                }
                show(false);
                continue;
            }
            const bool tutorials = page == MenuPage::Tutorials;
            StartupMenuSelection& list = tutorials ? tutorialSelection : selection;
            if (rocker[i] == board::kPress) {
                const StartupMenuItem& item = tutorials ? kTutorialMenuItems[list.selected] : kStartupMenuItems[list.selected];
                if (item.action == StartupMenuAction::Tutorials) page = MenuPage::Tutorials;
                else if (item.action == StartupMenuAction::Sounds) {
                    page = MenuPage::Sounds;
                    soundSelection = {};
                }
                else if (item.action == StartupMenuAction::Back) page = MenuPage::Modes;
                else if (tutorials) {
                    tutorialMode = item.mode;
                    tutorialPage = 0;
                    page = MenuPage::Slide;
                } else {
                    Serial.printf("menu: %s\n", modeName(item.mode));
                    return item.mode;
                }
            } else {
                list.select(list.selected + dir, tutorials ? kTutorialMenuCount : kStartupMenuCount);
            }
            show(false);
        }
        if (board::held(board::kSide)) {
            if (sideLetGo && sideSince < 0) sideSince = now;
            if (sideSince >= 0 && now - sideSince >= kPowerHoldUs) showOffPageAndPowerOff();
        } else {
            sideLetGo = true;
            sideSince = -1;
        }
        if (now - lastInput >= kMenuIdleUs) showOffPageAndPowerOff();
        delay(10);
    }
}

void activity(int64_t now) {
    lastActivity = now;
    if (sleepAt < now + kAwakeAfterPressUs) sleepAt = now + kAwakeAfterPressUs;
}

// The side button's last click, for taking it back when that press turns out to be a power-off hold.
Outcome sideClick;

void setScrollMode(bool scrolling) {
    timeHistory.scrolling = scrolling;
    timeHistory.offset = 0;
    Serial.printf("%s: scroll mode %s\n", modeName(mode), scrolling ? "on" : "off");
}

void onTimingPress(int64_t t, uint8_t b) {
    const bool wasScrolling = timeHistory.scrolling;
    const bool wasOnRocks = rules().focus && focus == Focus::Rocks;
    if (wasScrolling) setScrollMode(false);
    if (wasOnRocks) {
        if (rocksPage()) coachPage = 0;
        focusTo(Focus::Players);
        rocksFocusAt = -1;
        Serial.println("focus: players (timing)");
    }
    if (wasScrolling || wasOnRocks) {
        for (auto rocker : {board::kUp, board::kDown}) {
            if (buttons[rocker].down) buttons[rocker].holdFired = true;
        }
    }
    const Outcome o = timing.click(t);
    if (b == board::kSide) sideClick = o;
    if (o.event == TimingEvent::IgnoredTooSoon) playError();
    else sound.play(kClick, SoundKind::Click);
    handleOutcome(o, t);
    if ((wasScrolling || wasOnRocks) && o.event == TimingEvent::IgnoredTooSoon) syncModel();
    // Nothing to wait for between clicks: sleep as soon as the beep and screen are done.
    if (sleepWhileTiming || !timing.active()) sleepAt = t;
}

void onShot(bool made) {
    if (!shots.add(made)) { playError(); return; }
    addEntry(Entry::Shot);
    if (made) sound.play(kMade, SoundKind::Made); else sound.play(kMissed, SoundKind::Missed);
    Serial.printf("shots: %s, %d/%d\n", made ? "made" : "missed", shots.madeCount(), shots.count());
    // Each logged rock pins the throw order: which of the team's eight stones of which end it was
    // (league: one of your two; coach: the team's, in throwing order).
    const int64_t now = esp_timer_get_time();
    const int i = shots.count() - 1, perEnd = rules().shotsPerEnd;
    const int k = perEnd == 8 ? i % 8 : 2 * myPosition + i % 2;
    order.teamShot(i / perEnd, k, now);
    manualCursor = false;
    reaim(now);
    if (rules().autoFocus) {  // back to the columns, and your next rock planned
        focusTo(Focus::Players);
        planRocksFocus();
    }
    saveShots(secondsIn(now));
    if (!timing.active()) syncModel();
}

// Hold the rocker centre: undo the newest entry, a time or a shot, whichever came last (again every
// 0.5s while held). A stone being timed is cancelled first: if it has its B2H, that time goes too.
// The throw order steps back with it. Returns false when there was nothing to undo.
// Cancel the stone being timed. Returns true if its B2H is the newest entry, to be undone too
// (not if something, a coach's shot, was logged since).
bool cancelTiming(int64_t now) {
    const bool newestIsIt = timing.state() == TimingState::WaitingHH && entryCount > 0 &&
                            entries[entryCount - 1] == Entry::Stone;
    timing.reset();
    h2hSwitchAt = -1;
    closeStone(now);
    Serial.println("timing: cancelled");
    return newestIsIt;
}

// Take back the newest entry, a shot or a stone, and the throw order's step with it.
void undoEntry(int64_t now) {
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
}

bool onUndo() {
    const int64_t now = esp_timer_get_time();
    undoHint = false;  // the screen update below clears it
    if (timing.active() && !cancelTiming(now)) {
        sound.play(kUndo, SoundKind::Undo);
        reaim(now);
        syncModel();
        return true;
    }
    if (entryCount == 0) {
        playError();
        syncModel();  // clears "Keep holding to undo"
        return false;
    }
    undoEntry(now);
    sound.play(kUndo, SoundKind::Undo);
    syncModel();
    return true;
}

// The side button has been held long enough for "Keep holding to turn off": that press is the start
// of a power-off, not a timing click, so what its click did is taken back (at once: let go or not,
// that timing would never give a time). A stone it started (or
// restarted) is cancelled; a B2H it logged goes with its stone; an H2H it logged goes, the stone
// keeping its B2H if it had one.
void takeBackSideClick() {
    const int64_t now = esp_timer_get_time();
    const TimingEvent e = sideClick.event;
    const bool hadBh = sideClick.hadBh;
    sideClick = Outcome();
    if (e == TimingEvent::Started || e == TimingEvent::Restarted || e == TimingEvent::BH) {
        if (timing.active() && cancelTiming(now) && e == TimingEvent::BH) undoEntry(now);
        reaim(now);
    } else if (e == TimingEvent::HH) {
        if (hadBh) {  // the stone goes back to its B2H only
            if (stoneCount > 0) stones[0].hh = -1;
            bank.setLastHogToHog(StoneTimes::kNoTime, board::clockNow());
        } else if (entryCount > 0 && entries[entryCount - 1] == Entry::Stone) {
            undoEntry(now);  // an H2H-only stone: it goes
        }
        reaim(now);
    } else {
        return;
    }
    sound.play(kUndo, SoundKind::Undo);
    Serial.println("timing: side click taken back (it was the start of a power-off hold)");
}

// Coach: the other page. Page 2 is the rocks page (the rocker on the rocks), page 1 opens on the times.
void switchPage() {
    singleClickAt = -1;
    if (timeHistory.scrolling) setScrollMode(false);
    coachPage ^= 1;
    focus = coachPage == 1 ? Focus::Rocks : Focus::Players;
    rocksFocusAt = -1;
    Serial.printf("coach: page %d\n", coachPage + 1);
}

// A second short rocker press within 0.4s (not an undo): changes your position (lead, 2nd, 3rd,
// skip, round again), or a coach's page. Defer the single-click action until the window closes.
void onRockerPress(ButtonState& s, int64_t t) {
    if (t - lastRockerPress < kDoublePressUs) {
        s.holdFired = true;      // this press can't turn into an undo
        lastRockerPress = -kSec; // a third press starts over
        sound.play(kClick, SoundKind::Click);
        if (rules().pages) {
            switchPage();
        } else if (rules().positions) {
            if (timeHistory.scrolling) setScrollMode(false);
            myPosition = (myPosition + 1) % 4;
            prefs.putUChar("mypos", myPosition);
            restartOrder();
            Serial.printf("position: you are %s\n", kPositionNames[myPosition]);
            tipWaiting = true;  // show the new position once the card is on screen
            cardDismissAt = -1;  // (the first click's dismissal is off: the card stays up)
            tipUntil = -1;
        }
        syncModel();  // shows at once, even while a stone is timed
        return;
    }
    lastRockerPress = t;
}

// Basic: the rows a row down (+1, older stones) or up (-1), as far as there are rows (4 show).
void scrollBasic(int dir) {
    const bool fresh = timing.state() == TimingState::Started;  // a stone under way, no time yet: a row of its own
    const int most = stoneCount + (fresh ? 1 : 0) - 4;
    const int to = basicScroll + dir;
    if (to < 0 || to > (most > 0 ? most : 0)) { playError(); return; }
    basicScroll = to;
    sound.play(kClick, SoundKind::Click);
    Serial.printf("basic: scrolled %d\n", basicScroll);
    syncModel();
}

void scrollTimeHistory(int dir) {
    uint8_t shown[kVisibleTimeColumns];
    historyColumns(timeHistory, activeCol, columns(), shown);
    const int limit = historyScrollLimit(stones, stoneCount, seat(), shown);
    const int to = timeHistory.offset + dir;
    if (to < 0 || to > limit) { playError(); return; }
    timeHistory.offset = to;
    sound.play(kClick, SoundKind::Click);
    Serial.printf("%s: history offset %d\n", modeName(mode), timeHistory.offset);
    syncModel();
}

uint8_t shiftColumn(uint8_t column, int dir) {
    const int count = columns();
    if (rules().columns) {  // round the timeline, as it's drawn
        uint8_t cycle[kCoachColumns];
        timelineCycle(seat(), timelineRight, cycle);
        int i = 0;
        while (i < count - 1 && cycle[i] != column) ++i;
        const int j = (i + count + dir) % count;
        if (j > 2) timelineSide = static_cast<int8_t>(dir);  // beyond the three: the window slides the way you went
        return cycle[j];
    }
    return (column + count + dir) % count;
}

// The player cursor one column left (-1) or right (+1), wrapping at the edges.
void movePlayer(int dir) {
    activeCol = shiftColumn(activeCol, dir);
    manualCursor = true;
    rocksFocusAt = -1;  // you're on the columns on purpose
    sound.play(kClick, SoundKind::Click);
    const uint8_t p = columnPlayer(seat(), activeCol);
    Serial.printf("timing: %s next\n", p == kThem ? "THEM" : kPositionNames[p]);
    syncModel();
}

// Rocker up held on the columns: the newest time moves a column left every 0.7s, wrapping.
// Returns false when there's no time to move.
bool stepMove() {
    const int col = stoneCount > 0 ? playerColumn(seat(), stones[0].player) : -1;
    if (col < 0) { playError(); return false; }
    const uint8_t to = shiftColumn(col, -1);
    stones[0].player = columnPlayer(seat(), to);
    activeCol = to;
    ++moveSteps;
    sound.play(kClick, SoundKind::Click);
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

// Holding up on the rocks brings the rocker back to the timing columns.
void setFocus(Focus f) {
    if (f == Focus::Players && rocksPage()) coachPage = 0;
    focus = f;
    rocksFocusAt = -1;
    sound.play(kClick, SoundKind::Click);
    Serial.printf("focus: %s\n", f == Focus::Players ? "players" : "rocks");
    syncModel();
}

// A short press of rocker up/down acts on release: made/missed (coach, league on your rocks),
// next/previous player, or an older/newer row in Basic.
void releaseAction(uint8_t b);
void onRelease(uint8_t b) {
    // Coach: a single centre click shows the page hint after the double-click window.
    if (b == board::kPress && rules().pages && !buttons[b].holdFired && lastRockerPress >= 0 &&
        esp_timer_get_time() - lastRockerPress < kDoublePressUs) {
        singleClickPress = lastRockerPress;
        singleClickAt = lastRockerPress + kDoublePressUs;
    }
    const bool hadHint = moveHint && b == board::kUp;
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
    if (b == board::kUp && moveSteps > 0) { finishMove(); return; }
    if (buttons[b].holdFired) return;
    if (timeHistory.scrolling) scrollTimeHistory(b == board::kDown ? 1 : -1);
    else if (rules().shotsPerEnd && (!rules().focus || focus == Focus::Rocks)) onShot(b == board::kUp);
    else if (rules().columns) movePlayer(b == board::kUp ? -1 : 1);  // up = left, down = right
    else scrollBasic(b == board::kDown ? 1 : -1);  // basic: down = older stones, up = newer
}

// byPress: a rocker press dismissed it, so the rocker's presses are ignored until it's off the screen.
void dismissBasicIntro(bool byPress) {
    tipWaiting = false;
    tipUntil = -1;
    basicIntroClearing = byPress;
    syncModel();
}

void onPress(uint8_t b, int64_t t) {
    ButtonState& s = buttons[b];
    if (b != board::kPress) singleClickAt = -1;
    if (pageTipUntil >= 0) {  // any press: the page tip goes
        pageTipUntil = -1;
        syncModel();
    }
    s.down = true;
    s.downSince = t;
    s.holdFired = false;
    s.nextRepeat = t + kUndoHoldUs;
    if (b == board::kUp) moveSteps = 0;
    activity(t);
    if (mode == GameMode::Basic && (tipWaiting || tipUntil >= 0 || basicIntroClearing)) {
        // Any press dismisses the tutorial. A timing press also clicks, as with the position card:
        // the first stone's start isn't lost. A rocker press does nothing else (no hold or release
        // action), nor do its presses while the tutorial is still clearing.
        const bool timingPress = isTimingButton(b);
        if (tipWaiting || tipUntil >= 0) dismissBasicIntro(!timingPress);
        if (!timingPress) {
            s.holdFired = true;
            return;
        }
    }
    // The position card: any press dismisses it and still does its usual job; the rocker centre's
    // only once the double-click window has passed (cardDismissAt), as a double click shows it again.
    const bool cardUp = rules().positions && (tipWaiting || tipUntil >= 0);
    const bool dismissPosition = cardUp && b != board::kPress;
    if (cardUp && b == board::kPress) cardDismissAt = t + kDoublePressUs;
    if (dismissPosition) {
        tipWaiting = false;
        tipUntil = -1;
        cardDismissAt = -1;
    }
    // Rocker up/down act on release or hold. Position-card dismissal also happens on press.
    switch (b) {
    case board::kTop:
    case board::kSide: onTimingPress(t, b); break;
    case board::kPress: onRockerPress(s, t); break;
    default: break;
    }
    if (dismissPosition) syncModel();
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
                if (b == board::kSide && offHint) {  // let go before 3s: stays on (the click was taken back)
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
            // Rocker press: undo the newest entry after 1s, then again every 1s while held.
            // "Keep holding to undo" shows from 0.2s until the first undo only: shown again for the
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
                takeBackSideClick();  // a power-off hold, not a timing click: the stopwatch goes now
                syncModel();
            }
            if (b == board::kSide && now - s.downSince >= kPowerHoldUs) {
                s.holdFired = true;
                offHint = false;
                showOffPageAndPowerOff();
            }
            const bool downFromTimes = rules().columns && b == board::kDown &&
                                       (timeHistory.scrolling || focus == Focus::Players);
            const bool exitHistory = timeHistory.scrolling && b == board::kUp;
            if (raw && (downFromTimes || exitHistory) && now - s.downSince >= kScrollModeHoldUs) {
                s.holdFired = true;
                if (b == board::kDown && timeHistory.scrolling) {
                    if (rules().focus) {
                        setScrollMode(false);
                        activity(now);
                        setFocus(Focus::Rocks);
                    }
                    continue;
                }
                setScrollMode(b == board::kDown);
                if (b == board::kUp) {
                    focusTo(Focus::Players);
                    rocksFocusAt = -1;
                    if (!timing.active()) {  // re-aim: the order may have moved on, or restarted (a new position)
                        cursorToNext(now);
                        planFollow(now);
                    }
                }
                activity(now);
                sound.play(kClick, SoundKind::Click);
                syncModel();
                continue;
            }
            // In League and Coach, hold up on the rocks to return to the columns.
            // Holding up on the columns moves the newest time left.
            const bool rocker = b == board::kUp || b == board::kDown;
            const bool moves = !timeHistory.scrolling && rules().columns && b == board::kUp && raw &&
                               focus == Focus::Players;
            if (moves && !moveHint && stoneCount > 0 && now - s.downSince >= kMoveHintUs) {
                moveHint = true;  // a press of up/down is quicker: it never shows for one
                syncModel();
            }
            if (!timeHistory.scrolling && rules().focus && rocker && focus == Focus::Rocks &&
                now - s.downSince >= kFocusHoldUs) {
                s.holdFired = true;  // not a short press when it's let go
                activity(now);
                setFocus(b == board::kUp ? Focus::Players : Focus::Rocks);
            } else if (moves && now - s.downSince >= kMoveStepUs * (moveSteps + 1)) {
                // raw: no step once it's let go (it counts as released only after 20 ms up)
                activity(now);
                if (!stepMove()) s.holdFired = true;
            }
        }
    }
}

// Databank export, framed so an app can parse it: "@GAMES n", n JSON lines, "@END".
void printGames(uint32_t fromId) {
    printCrashes();  // first: kept with the games by an export
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
    } else if (!strcmp(buf, "sounds")) {
        saveSoundSettings();  // prints them
    } else if (!strcmp(buf, "sounds mute") || !strcmp(buf, "sounds on")) {
        soundSettings.muted = buf[7] == 'm';
        saveSoundSettings();
    } else if (!strcmp(buf, "n")) {
        newGame();
        syncModel();
    } else if (!strncmp(buf, "mode ", 5)) {  // testing: as if switched on in that mode
        const char* name = buf + 5;
        if (!strcmp(name, "competition")) mode = GameMode::Competition;
        else if (!strcmp(name, "league")) mode = GameMode::League;
        else if (!strcmp(name, "coach")) mode = GameMode::Coach;
        else if (!strcmp(name, "basic")) mode = GameMode::Basic;
        else { Serial.println("modes: basic | competition | league | coach"); return; }
        coachPage = 0;
        newGame();
        syncModel(board::Refresh::FastScreen);
    } else if (!strcmp(buf, "page")) {  // testing: coach page switch, as a double press
        if (rules().pages) { switchPage(); syncModel(); }
    } else if (!strcmp(buf, "o")) {
        showOffPageAndPowerOff();
    } else if (!strcmp(buf, "s")) {
        Serial.printf("status: board %s, %s mode, stones %d, shots %d/%d, games %lu-%lu, battery %d%%, clock %lu%s\n",
                      board::name(), modeName(mode), stoneCount, shots.madeCount(), shots.count(),
                      static_cast<unsigned long>(bank.firstId()), static_cast<unsigned long>(bank.lastId()),
                      board::batteryPercent(), static_cast<unsigned long>(board::clockNow()),
                      board::clockValid() ? "" : " (not set)");
    } else if (!strcmp(buf, "crash")) {
        printCrashes();
        Serial.printf("stack left: loop %u, display %u bytes\n", uxTaskGetStackHighWaterMark(nullptr),
                      uxTaskGetStackHighWaterMark(displayTaskHandle));
    } else if (!strcmp(buf, "crash clear")) {
        prefs.remove("crash");
        prefs.remove("crashes");
        prefs.remove("resets");
        prefs.remove("lastreset");
        crashNote[0] = 0;
        printCrashes();
    } else if (buf[0]) {
        Serial.println("commands: t <unix seconds> | s | games | games new | game <id> | ack <id> | bank | "
                       "erase-all yes | audit | crash | crash clear | sleeptiming on|off | sounds [mute|on] | n (new game) | "
                       "o (power off) | mode basic|competition|league|coach | page");
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
    noteCrashes();
    sleepWhileTiming = prefs.getBool("sleeptim", true);
    myPosition = prefs.getUChar("mypos", 0) % 4;
    loadSoundSettings(prefs);
    if (!bankStore.begin()) Serial.println("databank: no \"games\" partition, games won't be kept");
    bank.begin();
    // The simulator's first start is "off": the off page, until the side button switches it on (a
    // restart, picking the mode as below). Never on the CoreInk.
    if (board::startsOff()) showOffPageAndPowerOff();
    // The mode: the rocker held while the side button switches the device on picks it (down league,
    // up coach, middle competition); otherwise the menu.
    if (board::heldAtStart(board::kDown)) mode = GameMode::League;
    else if (board::heldAtStart(board::kUp)) mode = GameMode::Coach;
    else if (board::heldAtStart(board::kPress)) mode = GameMode::Competition;
    else mode = chooseMode();
    prefs.putUChar("menumode", static_cast<uint8_t>(mode));  // selected in the menu next time
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
    syncModel(board::Refresh::FastScreen);
    auditStart(esp_timer_get_time());  // first clock audit sample
    nextAuditAt = esp_timer_get_time() + kAuditEveryUs;
    Serial.printf("stoprock ready (%s, %s mode)\n", board::name(), modeName(mode));
}

void loop() {
    int64_t now = esp_timer_get_time();
    if (basicIntroClearing && !displayBusy) basicIntroClearing = false;
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
            syncModel(board::Refresh::Full);
        }
    }

    // Start the tutorial or position tip's timer once its frame has finished refreshing.
    if (tipWaiting && !displayBusy) {
        tipWaiting = false;
        tipUntil = now + (mode == GameMode::Basic ? kBasicIntroUs : kTipUs);
    }
    if (tipUntil >= 0 && now >= tipUntil) {
        if (mode == GameMode::Basic) dismissBasicIntro(false);
        else {
            tipUntil = -1;
            if (!timing.active()) syncModel();
        }
    }
    if (cardDismissAt >= 0 && now >= cardDismissAt) {  // a single click of the centre: the card goes
        cardDismissAt = -1;
        if (tipWaiting || tipUntil >= 0) {
            tipWaiting = false;
            tipUntil = -1;
            syncModel();
        }
    }
    if (h2hSwitchAt >= 0 && now >= h2hSwitchAt) {  // no B2H in time: it's an H2H under way
        h2hSwitchAt = -1;
        if (timing.state() == TimingState::Started) {
            Serial.println("timing: no B2H, an H2H under way");
            syncModel();
        }
    }
    if (singleClickAt >= 0 && now >= singleClickAt) {  // the window passed: a single click, or a double?
        singleClickAt = -1;
        if (lastRockerPress == singleClickPress && !buttons[board::kPress].down && !timeHistory.scrolling) {
            pageTipUntil = now + kPageTipUs;
            syncModel();
        }
    }
    if (pageTipUntil >= 0 && now >= pageTipUntil) {
        pageTipUntil = -1;
        syncModel();
    }
    if (!timeHistory.scrolling && followAt >= 0 && now >= followAt) {
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
    if (!timeHistory.scrolling && rocksFocusAt >= 0 && now >= rocksFocusAt) {  // your throw: ready to log it
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

    if (now - lastActivity >= kPowerOffUs) showOffPageAndPowerOff();

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
        if (!timeHistory.scrolling && rocksFocusAt >= 0 && rocksFocusAt < wake) wake = rocksFocusAt;
        if (!timeHistory.scrolling && followAt >= 0 && followAt < wake) wake = followAt;
        if (h2hSwitchAt >= 0 && h2hSwitchAt < wake) wake = h2hSwitchAt;
        if (singleClickAt >= 0 && singleClickAt < wake) wake = singleClickAt;
        if (pageTipUntil >= 0 && pageTipUntil < wake) wake = pageTipUntil;
        if (cardDismissAt >= 0 && cardDismissAt < wake) wake = cardDismissAt;
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
