#pragma once

#include "canvas.h"
#include "databank.h"

namespace stoprock {

struct StartupMenuItem {
    GameMode mode;
    const char* title;
    const char* description;
};

inline constexpr StartupMenuItem kStartupMenuItems[] = {
    {GameMode::Basic, "Basic", "Just the stopwatch"},
    {GameMode::Competition, "Competition", "Times only, by player"},
    {GameMode::League, "League", "Team times, your shots"},
    {GameMode::Coach, "Coach", "Track the whole team"},
};
inline constexpr int kStartupMenuCount = sizeof kStartupMenuItems / sizeof kStartupMenuItems[0];
inline constexpr int kStartupMenuVisibleRows = 3;

struct StartupMenuSelection {
    int selected = 0;
    int firstVisible = 0;

    // Wrap at the ends and scroll only when the selected row would leave the window.
    void select(int index, int count);
};

void drawStartupMenu(Canvas& c, const StartupMenuItem* items, int count, const StartupMenuSelection& selection);

}  // namespace stoprock
