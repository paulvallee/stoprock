#pragma once

#include "canvas.h"
#include "databank.h"
#include "sounds.h"

namespace stoprock {

enum class StartupMenuAction : uint8_t { Mode, Tutorials, Sounds, Back };

struct StartupMenuItem {
    GameMode mode;
    const char* title;
    const char* description;
    StartupMenuAction action = StartupMenuAction::Mode;
};

inline constexpr StartupMenuItem kStartupMenuItems[] = {
    {GameMode::Basic, "Basic", "Just the stopwatch"},
    {GameMode::Competition, "Competition", "Times only, by player"},
    {GameMode::League, "League", "Team times, your shots"},
    {GameMode::Coach, "Coach", "Track the whole team"},
    {GameMode::Basic, "Tutorials", "Learn the controls", StartupMenuAction::Tutorials},
    {GameMode::Basic, "Sounds", "Mute or set volumes", StartupMenuAction::Sounds},
};
inline constexpr int kStartupMenuCount = sizeof kStartupMenuItems / sizeof kStartupMenuItems[0];
inline constexpr StartupMenuItem kTutorialMenuItems[] = {
    {GameMode::Basic, "Basic", "Buttons and times"},
    {GameMode::Competition, "Competition", "Player times and history"},
    {GameMode::League, "League", "Times and your shots"},
    {GameMode::Coach, "Coach", "Times and the team log"},
    {GameMode::Basic, "Back", "Return to modes", StartupMenuAction::Back},
};
inline constexpr int kTutorialMenuCount = sizeof kTutorialMenuItems / sizeof kTutorialMenuItems[0];
inline constexpr int kStartupMenuVisibleRows = 3;
// The Sounds page: All sounds (row 0), a row for each SoundKind (rows 1..5), Back.
inline constexpr int kSoundMenuCount = 1 + kSoundKinds + 1;
inline constexpr const char* kSoundNames[kSoundKinds] = {"Clicks", "Made", "Missed", "Undo", "Error"};
inline constexpr const char* kSoundDetails[kSoundKinds] = {"Timing and buttons", "A rock made", "A rock missed",
                                                           "Taking a time back", "Too soon, can't do"};

struct StartupMenuSelection {
    int selected = 0;
    int firstVisible = 0;

    // Wrap at the ends and scroll only when the selected row would leave the window.
    void select(int index, int count);
};

void drawStartupMenu(Canvas& c, const StartupMenuItem* items, int count, const StartupMenuSelection& selection,
                     const Bitmap& heading = kLogo);
void drawSoundsMenu(Canvas& c, const SoundSettings& sounds, const StartupMenuSelection& selection);

}  // namespace stoprock
