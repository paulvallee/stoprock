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

// lastGame: e.g. "Last game 21 Nov   9/12   75%". battery: charge at switch-off, -1 = unknown.
void drawHelp(Canvas& c, const char* lastGame, int battery = -1);

// Hundredths of a second from microseconds, rounded.
inline int32_t toHundredths(int64_t us) { return static_cast<int32_t>((us + 5000) / 10000); }

}  // namespace stoprock
