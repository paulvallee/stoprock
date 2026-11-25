// The Tutorials decks: a deck of cards for each mode, each card a whole 200 x 200 picture drawn by
// tools/tutorials_claude/build.py and emitted by tools/tutorials.py (tutorial_data.cpp).
#pragma once

#include "canvas.h"
#include "databank.h"

namespace stoprock {

struct TutorialDeck {
    const Bitmap* const* cards;
    uint8_t count;
};

extern const Bitmap kTutorialsHeading;     // the Tutorials list's heading
extern const TutorialDeck kTutorialDecks[];  // indexed by GameMode

uint8_t tutorialPageCount(GameMode mode);
void drawTutorial(Canvas& c, GameMode mode, uint8_t page);

}  // namespace stoprock
