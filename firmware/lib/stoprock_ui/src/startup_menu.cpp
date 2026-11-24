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

void drawControls(Canvas& c) {
    c.line(26, 190, 29, 187);
    c.line(29, 187, 32, 190);
    c.line(26, 192, 29, 195);
    c.line(29, 195, 32, 192);
    c.text(kFontMenuDetail, 38, 183, "Scroll");
    c.fillRect(93, 188, 95, 194);
    c.fillRect(91, 190, 97, 192);
    c.fillRect(92, 189, 96, 193);
    c.text(kFontMenuDetail, 104, 183, "Press to open");
}

}  // namespace

void drawStartupMenu(Canvas& c, const StartupMenuItem* items, int count, const StartupMenuSelection& selection) {
    c.clear();
    c.blit(kLogo, (Canvas::W - kLogo.w) / 2, 4);
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
        if (selected) {
            for (int offset = 0; offset < 2; ++offset) {
                c.line(170 + offset, top + 15, 175 + offset, top + 20, 1, false);
                c.line(175 + offset, top + 20, 170 + offset, top + 25, 1, false);
            }
        }
    }
    drawScrollBar(c, selection.firstVisible, count);
    drawControls(c);
}

}  // namespace stoprock
