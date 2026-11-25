#include "tutorials.h"

#include <stdio.h>

namespace stoprock {

uint8_t tutorialPageCount(GameMode mode) {
    return kTutorialDecks[static_cast<uint8_t>(mode)].count;
}

void drawTutorial(Canvas& c, GameMode mode, uint8_t page) {
    const TutorialDeck& deck = kTutorialDecks[static_cast<uint8_t>(mode)];
    const TutorialSlide& slide = kTutorialSlides[deck.slides[page]];
    const char* name = "Basic";
    switch (mode) {
    case GameMode::Competition: name = "Competition"; break;
    case GameMode::League: name = "League"; break;
    case GameMode::Coach: name = "Coach"; break;
    case GameMode::Basic: break;
    }

    c.clear();
    c.text(kFontMenuDetail, 10, 4, name);
    char progress[8];
    snprintf(progress, sizeof progress, "%u/%u", page + 1u, static_cast<unsigned>(deck.count));
    c.text(kFontMenuDetail, 190 - c.ink(kFontMenuDetail, progress).r, 4, progress);
    const InkBox title = c.ink(kFontMenuTitle, slide.title);
    c.text(kFontMenuTitle, (Canvas::W - (title.r - title.l)) / 2 - title.l, 20, slide.title);
    c.fillRect(10, 42, 189, 42);
    c.blit(slide.illustration, 0, 43);
    c.fillRect(0, 174, Canvas::W - 1, Canvas::H - 1);
    c.text(kFontMenuDetail, 10, 180, "Up/down: pages", false);
    const char* back = "Centre: back";
    c.text(kFontMenuDetail, 190 - c.ink(kFontMenuDetail, back).r, 180, back, false);
}

}  // namespace stoprock
