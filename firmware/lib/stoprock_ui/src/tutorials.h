#pragma once

#include "canvas.h"
#include "databank.h"

namespace stoprock {

struct TutorialSlide {
    const char* title;
    Bitmap illustration;
};
struct TutorialDeck {
    const uint8_t* slides;
    uint8_t count;
};

extern const Bitmap kTutorialsHeading;
extern const TutorialSlide kTutorialSlides[];
extern const TutorialDeck kTutorialDecks[];

uint8_t tutorialPageCount(GameMode mode);
void drawTutorial(Canvas& c, GameMode mode, uint8_t page);

}  // namespace stoprock
