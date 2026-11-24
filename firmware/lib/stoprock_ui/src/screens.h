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
// HogToHogOnly: 5.5 s after the first click with no second, it can only be an H2H (the first click
// was at the near hog line): the row is still the new stone's, the stopwatch in the H2H box.
enum class Waiting : uint8_t { None, BackToHog, HogToHog, HogToHogOnly };

// A box over the times: the switch-on tip (your position and the holds), or while the rocker centre
// is held, "Keep holding to undo" (every screen, coach page 2 too).
enum class Tip : uint8_t { None, SwitchOn, Undo };

// lastGame: e.g. "Last game 21 Nov   9/12". shotPct: shown with a pie, -1 = none.
// battery: charge at switch-off, -1 = unknown. batteryNote: left of the battery, e.g.
// "~11 games battery left"; nullptr or "" = none.
void drawHelp(Canvas& c, const char* lastGame, int shotPct = -1, int battery = -1,
              const char* batteryNote = nullptr);

// Competition (switched on with the side button alone): times only, and a badge strip an umpire
// can read. lowBattery replaces the strip's second line. The history is as in league (below): four
// columns, your teammates and THEM, 8 lines; activeCol (0-3) has its header highlighted.
constexpr uint8_t kCompetitionRows = 8;
constexpr uint8_t kGridRows = 8;  // stones kept per player (addStone)
// tip: the switch-on tip (your position, double click to change it, hold up to move) over the times.
void drawCompetition(Canvas& c, const Stone* stones, uint8_t count, uint8_t me, uint8_t activeCol,
                     bool lowBattery, Tip tip = Tip::None, Waiting waiting = Waiting::None);

// League (side button + rocker down): the newest stone large, then four columns: your three
// teammates in throwing order and the other team (THEM), each listing its stones newest first, a
// time a line (7 lines): B2H black on white, H2H white on black. Below, your own 16 rocks like a
// scoreboard (ends across, rock 1 over rock 2) and your percentage. activeCol (0-3) has its header
// highlighted. lowBattery puts a "Batt low" bar over the percentage.
// Columns and who's in them: seats.h (kLeagueColumns, columnPlayer, playerColumn).
constexpr uint8_t kLeagueRows = 7;
constexpr uint8_t kCoachRows = 6;  // coach page 1: five columns (kCoachColumns), seat kCoachSeat
// focus: the half the rocker works on, shown by a bold border: the players' columns (up = next,
// down = previous) or your rocks (made/missed).
enum class Focus : uint8_t { Rocks, Players };
void drawLeague(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots, uint8_t me, uint8_t activeCol,
                bool lowBattery, Tip tip = Tip::None, Waiting waiting = Waiting::None, Focus focus = Focus::Players);

// Put a stone at the front of a newest-first list. When the list is full, the stone dropped is
// the oldest of a player (or THEM) with more than kGridRows, so a player who has thrown less
// keeps their times. With no players (coach) it simply drops the oldest.
void addStone(Stone* list, uint8_t& count, uint8_t capacity, const Stone& s);

// Coach (side button + rocker up), 64 rocks: 8 ends x 4 positions x 2.
// Page 1, timer priority: the newest stone large, a column per position and THEM (6 lines, narrow
// digits; activeCol 0-4 highlighted), % per position, the previous and current end. tip: the
// switch-on tip (double click to switch page, hold up to move the last time).
void drawCoachTimer(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots,
                    Waiting waiting = Waiting::None, uint8_t activeCol = 0, Tip tip = Tip::None);
// Page 2, rock priority: the newest times only, all 64 rocks, a pie and % per position.
void drawCoachRocks(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots,
                    Waiting waiting = Waiting::None, Tip tip = Tip::None);

// Hundredths of a second from microseconds, rounded.
inline int32_t toHundredths(int64_t us) { return static_cast<int32_t>((us + 5000) / 10000); }

}  // namespace stoprock
