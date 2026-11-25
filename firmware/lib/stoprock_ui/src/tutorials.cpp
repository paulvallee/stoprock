#include "tutorials.h"

namespace stoprock {

uint8_t tutorialPageCount(GameMode mode) {
    return kTutorialDecks[static_cast<uint8_t>(mode)].count;
}

void drawTutorial(Canvas& c, GameMode mode, uint8_t page) {
    const TutorialDeck& deck = kTutorialDecks[static_cast<uint8_t>(mode)];
    c.clear();
    if (page < deck.count) c.blit(*deck.cards[page], 0, 0);
}

}  // namespace stoprock
