// The stoprock screens, drawn to match tools/mockups/screen.py (docs/SPEC.md sections 8 and 9).
#pragma once
#include <stdint.h>

#include "canvas.h"
#include "shots.h"

namespace stoprock {

// One timed stone. Times in hundredths of a second; -1 = that split wasn't timed.
struct Stone {
    int32_t bh = -1;
    int32_t hh = -1;
    uint8_t player = kNoPlayer;  // competition and league: who threw it, 0 lead .. 3 skip
    static constexpr uint8_t kNoPlayer = 0xFF;
};

// While a stone is being timed: which time is awaited. The screens show a stopwatch where it will
// appear; awaiting the B2H, the large row is the new stone's (empty) and the last stone counts as older.
enum class Waiting : uint8_t { None, BackToHog, HogToHog };

// lastGame: e.g. "Last game 21 Nov   9/12". shotPct: shown with a pie, -1 = none.
// battery: charge at switch-off, -1 = unknown. batteryNote: left of the battery, e.g.
// "~11 games battery left"; nullptr or "" = none.
void drawHelp(Canvas& c, const char* lastGame, int shotPct = -1, int battery = -1,
              const char* batteryNote = nullptr);

// Competition (switched on with the side button alone): times only, full width, and a badge
// strip an umpire can read. lowBattery replaces the strip's second line. The history has a column
// per teammate (the three positions other than me, 0 lead .. 3 skip, in throwing order), each
// listing that player's stones newest first; activeCol (0-2) has its header highlighted.
constexpr uint8_t kGridColumns = 3, kGridRows = 8;
// tip: the switch-on tip, "Double click rocker to change position", in a box over the times.
void drawCompetition(Canvas& c, const Stone* stones, uint8_t count, uint8_t me, uint8_t activeCol,
                     bool lowBattery, bool tip = false, Waiting waiting = Waiting::None);
// The position shown in column k (0-2) for a player at position me.
inline uint8_t teammate(uint8_t me, uint8_t k) { return k < me ? k : k + 1; }

// League (side button + rocker down): the competition layout with 7 rows a player, and below it
// your own 16 rocks like a scoreboard (ends across, rock 1 over rock 2) and your percentage.
// lowBattery puts a "Batt low" bar over the percentage.
constexpr uint8_t kLeagueRows = 7;
// focus: the half the rocker works on, shown by a bold border: your rocks (made/missed) or the
// players (up = next player, down = previous).
enum class Focus : uint8_t { Rocks, Players };
void drawLeague(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots, uint8_t me, uint8_t activeCol,
                bool lowBattery, bool tip = false, Waiting waiting = Waiting::None, Focus focus = Focus::Rocks);

// Put a stone at the front of a newest-first list. When the list is full, the stone dropped is
// the oldest of a player with more than a screenful (kGridRows), so a player who has thrown
// less keeps their times. With no players (league, coach) it simply drops the oldest.
void addStone(Stone* list, uint8_t& count, uint8_t capacity, const Stone& s);

// Coach (side button + rocker up), 64 rocks: 8 ends x 4 positions x 2.
// Page 1, timer priority: wide times, % per position, the previous and current end.
void drawCoachTimer(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots,
                    Waiting waiting = Waiting::None);
// Page 2, rock priority: the newest times only, all 64 rocks, a pie and % per position.
void drawCoachRocks(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots,
                    Waiting waiting = Waiting::None);

// Hundredths of a second from microseconds, rounded.
inline int32_t toHundredths(int64_t us) { return static_cast<int32_t>((us + 5000) / 10000); }

}  // namespace stoprock
