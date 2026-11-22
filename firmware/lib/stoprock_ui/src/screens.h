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
};

struct MainView {
    const Stone* stones = nullptr;  // newest first
    uint8_t stoneCount = 0;
    const ShotLog* shots = nullptr;
    const char* tip1 = nullptr;     // tip strip; nullptr = no strip
    const char* tip2 = nullptr;
    bool warning = false;           // white-on-black strip
};

void drawMain(Canvas& c, const MainView& v);

// lastGame: e.g. "Last game 21 Nov   9/12". shotPct: shown with a pie, -1 = none.
// battery: charge at switch-off, -1 = unknown. batteryNote: left of the battery, e.g.
// "~11 games battery left"; nullptr or "" = none.
void drawHelp(Canvas& c, const char* lastGame, int shotPct = -1, int battery = -1,
              const char* batteryNote = nullptr);

// Competition (switched on with the side button alone): times only, full width, and a badge
// strip an umpire can read. lowBattery replaces the strip's second line.
void drawCompetition(Canvas& c, const Stone* stones, uint8_t count, bool lowBattery);

// Coach (side button + rocker up), 64 rocks: 8 ends x 4 positions x 2.
// Page 1, timer priority: wide times, % per position, the previous and current end.
void drawCoachTimer(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots);
// Page 2, rock priority: the newest times only, all 64 rocks, a pie and % per position.
void drawCoachRocks(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots);

// Hundredths of a second from microseconds, rounded.
inline int32_t toHundredths(int64_t us) { return static_cast<int32_t>((us + 5000) / 10000); }

}  // namespace stoprock
