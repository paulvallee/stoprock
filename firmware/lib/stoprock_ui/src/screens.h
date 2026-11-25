// The stoprock screens, drawn to match tools/mockups/screen.py (docs/SPEC.md sections 8 and 9).
#pragma once
#include <stdint.h>

#include "canvas.h"
#include "seats.h"
#include "shots.h"

namespace stoprock {

// One timed stone. Times in hundredths of a second; -1 = that split wasn't timed.
struct Stone {
    int32_t bh = -1;
    int32_t hh = -1;
    uint8_t player = kNoPlayer;  // who threw it: 0 lead .. 3 skip, or kThem (player codes: seats.h)
    static constexpr uint8_t kNoPlayer = stoprock::kNoPlayer;
    static constexpr uint8_t kThem = stoprock::kThem;
};

// While a stone is being timed: which time is awaited. The screens show a stopwatch where it will
// appear; awaiting the B2H, the large row is the new stone's (empty) and the last stone counts as older.
// HogToHogOnly: 7s after the first click with no second, it can only be an H2H (the first click
// was at the near hog line): the row is still the new stone's, the stopwatch in the H2H box.
enum class Waiting : uint8_t { None, BackToHog, HogToHog, HogToHogOnly };

// A box over the times: the switch-on tip (your position and the holds), or while the rocker centre
// is held, "Keep holding to undo" (every screen, coach page 2 too). Move: while the rocker is held
// to move the newest time, "Keep holding to move time" over the bottom (the rocks, or competition's
// badge strip), clear of the columns.
// PowerOff: while the side button is held, before it switches off, "Keep holding to turn off" (where
// the undo one goes).
// Page: coach, after a single click of the rocker centre, "Double click rocker middle / to change pages".
// BasicIntro: full-screen introduction showing the two physical timing buttons.
enum class Tip : uint8_t { None, SwitchOn, Undo, Move, PowerOff, Page, BasicIntro };

// The wake screen shows games left when known, or charge percentage. -1 means unavailable.
void drawOffPage(Canvas& c, int battery = -1, int gamesLeft = -1);

// Basic: times only, no players, a row a stone (4), as big as they go; the badge strip below. scroll:
// rows scrolled down, to older stones (the badge then says which show, e.g. "5-8 of 14").
void drawBasic(Canvas& c, const Stone* stones, uint8_t count, bool lowBattery, Tip tip = Tip::None,
               Waiting waiting = Waiting::None, int scroll = 0);

// Three player columns, with a separate offset for older split rows. League and coach show three
// adjacent columns from first. Competition shows them in throw order (timeline): our last thrower,
// THEM, our next (visible, from timelineColumns()); all headers alike, in bold Spleen 12x24.
constexpr uint8_t kVisibleTimeColumns = 3;
struct TimeHistoryView {
    uint8_t first = 0;
    int offset = 0;
    bool scrolling = false;
    bool timeline = false;
    uint8_t visible[kVisibleTimeColumns] = {0, 0, 0};
};
constexpr uint8_t firstVisibleTimeColumn(uint8_t first, uint8_t active, uint8_t total) {
    const uint8_t last = total - kVisibleTimeColumns;
    if (first > last) first = last;
    if (active < first) return active;
    if (active >= first + kVisibleTimeColumns) return active + 1 - kVisibleTimeColumns;
    return first;
}
// Competition's timeline: left (the teammate column of our last thrower), THEM, right (our next). With
// the cursor (active) on the fourth column, the window slides to it: forward [THEM, right, it] or
// back [it, left, THEM]. me: your position (THEM is column 3).
void timelineColumns(uint8_t me, uint8_t left, uint8_t right, uint8_t active, bool forward,
                     uint8_t out[kVisibleTimeColumns]);
// The columns a view shows, left to right.
void historyColumns(const TimeHistoryView& view, uint8_t active, uint8_t total, uint8_t out[kVisibleTimeColumns]);
constexpr uint8_t kGridRows = 8;  // stones kept per player (addStone)
int historyScrollLimit(const Stone* stones, uint8_t count, uint8_t seat, const uint8_t visible[kVisibleTimeColumns]);
void drawCompetition(Canvas& c, const Stone* stones, uint8_t count, uint8_t me, uint8_t activeCol,
                     bool lowBattery, Tip tip = Tip::None, Waiting waiting = Waiting::None,
                     TimeHistoryView history = {});

// League: timing history above the player's 16 rocks and their percentage.
enum class Focus : uint8_t { Rocks, Players };
void drawLeague(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots, uint8_t me, uint8_t activeCol,
                bool lowBattery, Tip tip = Tip::None, Waiting waiting = Waiting::None, Focus focus = Focus::Players,
                TimeHistoryView history = {});

// Put a stone at the front of a newest-first list. When the list is full, the stone dropped is
// the oldest of a player (or THEM) with more than kGridRows, so a player who has thrown less
// keeps their times. With no players (coach) it simply drops the oldest.
void addStone(Stone* list, uint8_t& count, uint8_t capacity, const Stone& s);

// Coach (side button + rocker up), 64 rocks: 8 ends x 4 positions x 2.
// Page 1: timing history above percentages and the previous and current ends.
void drawCoachTimer(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots,
                    Waiting waiting = Waiting::None, uint8_t activeCol = 0, Tip tip = Tip::None,
                    Focus focus = Focus::Players, TimeHistoryView history = {});
// Page 2, rock priority: the newest times only, all 64 rocks, a pie and % per position.
void drawCoachRocks(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots,
                    Waiting waiting = Waiting::None, Tip tip = Tip::None, Focus focus = Focus::Players);

// Hundredths of a second from microseconds, rounded.
inline int32_t toHundredths(int64_t us) { return static_cast<int32_t>((us + 5000) / 10000); }

}  // namespace stoprock
