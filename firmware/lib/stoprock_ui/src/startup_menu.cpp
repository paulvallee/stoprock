#include "startup_menu.h"

#include <algorithm>

namespace stoprock {

void StartupMenuSelection::select(int index, int count) {
    if (count <= 0) {
        *this = {};
        return;
    }
    selected = index % count;
    if (selected < 0) selected += count;
    const int lastWindow = std::max(0, count - kStartupMenuVisibleRows);
    firstVisible = std::clamp(firstVisible, 0, lastWindow);
    if (selected < firstVisible) firstVisible = selected;
    if (selected >= firstVisible + kStartupMenuVisibleRows)
        firstVisible = selected - kStartupMenuVisibleRows + 1;
}

namespace {

constexpr int kListTop = 51, kRowPitch = 44, kRowHeight = 40;
constexpr int kCardLeft = 10, kCardRight = 184, kTextLeft = 20;

void roundedCard(Canvas& c, int left, int top, int right, int bottom, bool black) {
    constexpr int insets[] = {3, 2, 1};
    for (int row = 0; row < 3; ++row) {
        const int inset = insets[row];
        c.fillRect(left + inset, top + row, right - inset, top + row, black);
        c.fillRect(left + inset, bottom - row, right - inset, bottom - row, black);
    }
    c.fillRect(left, top + 3, right, bottom - 3, black);
}

void drawScrollBar(Canvas& c, int firstVisible, int count) {
    if (count <= kStartupMenuVisibleRows) return;
    constexpr int top = kListTop + 2;
    constexpr int height = (kStartupMenuVisibleRows - 1) * kRowPitch + kRowHeight - 4;
    const int thumb = std::max(12, height * kStartupMenuVisibleRows / count);
    const int offset = firstVisible * (height - thumb) / (count - kStartupMenuVisibleRows);
    c.fillRect(192, top, 192, top + height - 1);
    c.fillRect(190, top + offset, 194, top + offset + thumb - 1);
}

// The cards under a heading, three at a time. extra(c, index, top, selected) draws a row's right-hand
// side and returns true; otherwise the selected row gets the chevron.
template <typename Extra>
void drawList(Canvas& c, const StartupMenuItem* items, int count, const StartupMenuSelection& selection,
              const Bitmap& heading, Extra extra) {
    c.clear();
    c.blit(heading, (Canvas::W - heading.w) / 2, 4);
    for (int row = 0; row < kStartupMenuVisibleRows; ++row) {
        const int index = selection.firstVisible + row;
        if (index >= count) break;
        const int top = kListTop + row * kRowPitch;
        const int bottom = top + kRowHeight - 1;
        const bool selected = index == selection.selected;
        const StartupMenuItem& item = items[index];

        roundedCard(c, kCardLeft, top, kCardRight, bottom, true);
        if (!selected) roundedCard(c, kCardLeft + 1, top + 1, kCardRight - 1, bottom - 1, false);
        c.text(kFontMenuTitle, kTextLeft, top + 3, item.title, !selected);
        c.text(kFontMenuDetail, kTextLeft, top + 22, item.description, !selected);
        if (extra(c, index, top, selected)) continue;
        if (selected) {
            for (int offset = 0; offset < 2; ++offset) {
                c.line(170 + offset, top + 15, 175 + offset, top + 20, 1, false);
                c.line(175 + offset, top + 20, 170 + offset, top + 25, 1, false);
            }
        }
    }
    drawScrollBar(c, selection.firstVisible, count);
}

// Four bars rising left to right, the first `level` filled, the rest outlined.
void drawMeter(Canvas& c, int top, uint8_t level, bool black) {
    for (int i = 0; i < kSoundLevels; ++i) {
        const int x0 = 147 + 7 * i, x1 = x0 + 4, y1 = top + 28, y0 = y1 - (5 + 4 * i) + 1;
        if (i < level) {
            c.fillRect(x0, y0, x1, y1, black);
        } else {
            c.fillRect(x0, y0, x1, y0, black);
            c.fillRect(x0, y1, x1, y1, black);
            c.fillRect(x0, y0, x0, y1, black);
            c.fillRect(x1, y0, x1, y1, black);
        }
    }
}

}  // namespace

void drawStartupMenu(Canvas& c, const StartupMenuItem* items, int count, const StartupMenuSelection& selection,
                     const Bitmap& heading) {
    drawList(c, items, count, selection, heading, [](Canvas&, int, int, bool) { return false; });
}

void drawSoundsMenu(Canvas& c, const SoundSettings& sounds, const StartupMenuSelection& selection) {
    StartupMenuItem items[kSoundMenuCount];
    items[0] = {GameMode::Basic, "All sounds", sounds.muted ? "Muted" : "Press to mute"};
    for (int k = 0; k < kSoundKinds; ++k)
        items[1 + k] = {GameMode::Basic, kSoundNames[k], sounds.muted ? "All muted" : kSoundDetails[k]};
    items[kSoundMenuCount - 1] = {GameMode::Basic, "Back", "Return to modes", StartupMenuAction::Back};
    drawList(c, items, kSoundMenuCount, selection, kSoundsHeading, [&](Canvas& cv, int index, int top, bool selected) {
        if (index == 0) {
            cv.blit(sounds.muted ? kSoundSwitchOff : kSoundSwitchOn, 146, top + 12, !selected);
            return true;
        }
        if (index <= kSoundKinds) {
            drawMeter(cv, top, sounds.level[index - 1], !selected);
            return true;
        }
        return false;
    });
}

}  // namespace stoprock
