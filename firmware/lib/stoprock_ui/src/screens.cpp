// Port of tools/mockups/screen.py. Keep the two in step: the mockups are the reference.
#include "screens.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace stoprock {
namespace {

constexpr int W = 200, H = 200;
constexpr int TIP_H = 26;  // competition's badge strip

// Python's round(): halves go to the even neighbour.
int pyRound(double v) {
    double f = floor(v), d = v - f;
    if (d > 0.5) return static_cast<int>(f) + 1;
    if (d < 0.5) return static_cast<int>(f);
    return (static_cast<long>(f) % 2 == 0) ? static_cast<int>(f) : static_cast<int>(f) + 1;
}

// ------------------------------------------------------------------ drawn seven-segment digits
struct SegFont {
    int h, w, t, gap, dot;
    bool prop;
    bool solidOne = false;  // "1" as one stroke: at big sizes the gap between its two reads as "!"

    int charWidth(char ch) const {
        if (ch == '.') return dot;
        if (ch == '1' && prop) return t;
        return w;
    }
    int width(const char* s) const {
        int n = 0, total = 0;
        for (; *s; ++s, ++n) total += charWidth(*s);
        return n ? total + gap * (n - 1) : 0;
    }
    void digit(Canvas& c, char ch, int x, int y, bool black = true) const {
        static const char* const kSegs[10] = {"abcdef", "bc", "abged", "abgcd", "fgbc",
                                              "afgcd", "afgedc", "abc", "abcdefg", "abcdfg"};
        if (ch < '0' || ch > '9') return;
        const char* segs = kSegs[ch - '0'];
        const int m = (h - t) / 2, k = (t == 1) ? 1 : t - 1, extra = (t == 1) ? 1 : 0;
        auto r = [&](int x0, int y0, int x1, int y1) { c.fillRect(x + x0, y + y0, x + x1 - 1, y + y1 - 1, black); };
        if (ch == '1' && solidOne) {
            r(w - t, k, w, h - k);
            return;
        }
        if (strchr(segs, 'a')) r(k, 0, w - k, t);
        if (strchr(segs, 'g')) r(k, m, w - k, m + t);
        if (strchr(segs, 'd')) r(k, h - t, w - k, h);
        if (strchr(segs, 'f')) r(0, k, t, m + t - k + extra);
        if (strchr(segs, 'b')) r(w - t, k, w, m + t - k + extra);
        if (strchr(segs, 'e')) r(0, m + k, t, h - k);
        if (strchr(segs, 'c')) r(w - t, m + k, w, h - k);
    }
    void draw(Canvas& c, const char* s, int x, int y, bool black = true) const {
        for (; *s; ++s) {
            if (*s == '.') c.fillRect(x, y + h - dot, x + dot - 1, y + h - 1, black);
            else if (*s == '1' && prop) digit(c, *s, x - (w - t), y, black);
            else digit(c, *s, x, y, black);
            x += charWidth(*s) + gap;
        }
    }
};

constexpr SegFont kEndFont{9, 5, 1, 1, 1, false};   // end numbers: equal-width cells, a "1" sits on the right

enum Align { kLeft, kCentre, kRight };

void textInBox(Canvas& c, const Font& f, const char* s, double x0, double x1, double y0, double y1,
               Align a, double margin, bool black = true) {
    InkBox b = c.ink(f, s);
    const double w = b.r - b.l;
    double x = a == kRight ? x1 - margin - w - b.l : a == kLeft ? x0 + margin - b.l : (x0 + x1 - w) / 2 - b.l;
    double y = (y0 + y1 - (b.b - b.t)) / 2 - b.t;
    c.text(f, pyRound(x), pyRound(y), s, black);
}

void segInBox(Canvas& c, const SegFont& f, const char* s, double x0, double x1, double y0, double y1,
              Align a, double margin, bool black = true) {
    const int w = f.width(s);
    double x = a == kRight ? x1 - margin - w : a == kLeft ? x0 + margin : floor((x0 + x1 - w) / 2);
    double y = floor((y0 + y1 - f.h) / 2);
    f.draw(c, s, static_cast<int>(x), static_cast<int>(y), black);
}

void formatTime(int32_t hundredths, char* out, size_t n) {
    snprintf(out, n, "%ld.%02ld", static_cast<long>(hundredths / 100), static_cast<long>(hundredths % 100));
}

// DSEG7 number with a small square decimal point, right-aligned (or centred) in the box.
void dsegNumber(Canvas& c, int32_t hundredths, double x0, double x1, double y0, double y1, double margin,
                const Font& f, bool center = false, bool black = true) {
    char ip[12], fp[4];
    snprintf(ip, sizeof ip, "%ld", static_cast<long>(hundredths / 100));
    snprintf(fp, sizeof fp, "%02ld", static_cast<long>(hundredths % 100));
    InkBox bi = c.ink(f, ip), bf = c.ink(f, fp);
    const int t = bi.t < bf.t ? bi.t : bf.t, b = bi.b > bf.b ? bi.b : bf.b;
    const int dot = pyRound((b - t) * 0.13) > 3 ? pyRound((b - t) * 0.13) : 3;
    const int gap = 2;
    const double total = (bi.r - bi.l) + gap + dot + gap + (bf.r - bf.l);
    const double x = center ? (x0 + x1 - total) / 2 : x1 - margin - total;
    const double y = (y0 + y1 - (b - t)) / 2.0 - t;
    c.text(f, pyRound(x - bi.l), pyRound(y), ip, black);
    const double xd = x + (bi.r - bi.l) + gap;
    const double yb = y + b;
    c.fillRect(pyRound(xd), pyRound(yb - dot), pyRound(xd + dot - 1), pyRound(yb - 1), black);
    c.text(f, pyRound(xd + dot + gap - bf.l), pyRound(y), fp, black);
}

// DSEG7 16 px digits packed by their ink and a % sign drawn to match, centred in the box.
void bigPercent(Canvas& c, int pct, double x0, double x1, double y0, double y1) {
    char n[4];
    snprintf(n, sizeof n, "%d", pct < 0 ? 0 : pct > 100 ? 100 : pct);
    const Font& f = kFontDseg16;
    const int len = static_cast<int>(strlen(n));
    InkBox boxes[3];
    int t = 1000, b = -1000, inkw = 0;
    for (int i = 0; i < len; ++i) {
        char one[2] = {n[i], 0};
        boxes[i] = c.ink(f, one);
        if (boxes[i].t < t) t = boxes[i].t;
        if (boxes[i].b > b) b = boxes[i].b;
        inkw += boxes[i].r - boxes[i].l;
    }
    const int dh = b - t, pw = pyRound(dh * 0.6), q = 3;
    const int gap = len > 2 ? 2 : 3;
    const double total = inkw + gap * len + pw;
    double x = (x0 + x1 - total) / 2;
    const double ytop = (y0 + y1 - dh) / 2;
    for (int i = 0; i < len; ++i) {
        char one[2] = {n[i], 0};
        c.text(f, pyRound(x - boxes[i].l), pyRound(ytop - t), one);
        x += (boxes[i].r - boxes[i].l) + gap;
    }
    const int px = pyRound(x), py = pyRound(ytop);
    c.fillRect(px, py, px + q - 1, py + q - 1);
    c.fillRect(px + pw - q, py + dh - q, px + pw - 1, py + dh - 1);
    const Sprite* slash = nullptr;
    for (uint8_t i = 0; i < kPercentSlashCount; ++i)
        if (kPercentSlashes[i].digitHeight == dh) slash = kPercentSlashes[i].sprite;
    if (slash) c.sprite(*slash, px, py);
    else c.line(px + pw - 1, py, px, py + dh - 1, 2);
}

void hline(Canvas& c, int x0, int x1, int y, int width = 1) { c.fillRect(x0, y, x1, y + width - 1); }
void vline(Canvas& c, int x, int y0, int y1, int width = 1) { c.fillRect(x, y0, x + width - 1, y1); }

}  // namespace

// 11 px pie: a 1-px ring and, inside a 1-px gap, a wedge filled clockwise from 12 o'clock.
static void drawPie(Canvas& c, int cx, int cy, int pct) {
    for (int dy = -6; dy <= 6; ++dy) {
        for (int dx = -6; dx <= 6; ++dx) {
            const int d2 = dx * dx + dy * dy;
            if (d2 > 20 && d2 <= 30) {
                c.set(cx + dx, cy + dy);
            } else if (d2 <= 13 && pct > 0) {  // 1-px gap between wedge and ring
                double a = atan2(static_cast<double>(dx), static_cast<double>(-dy));
                if (a < 0) a += 2 * M_PI;
                if (pct >= 100 || a < pct / 100.0 * 2 * M_PI) c.set(cx + dx, cy + dy);
            }
        }
    }
}

void drawOffPage(Canvas& c, int battery, int gamesLeft) {
    c.clear();
    c.blit(kOffPage, 0, 0);
    if (battery < 0) return;
    if (battery > 100) battery = 100;

    // The empty lower-left area under the curve leaves the wake arrow untouched.
    c.fillRect(12, 182, 27, 191);
    c.fillRect(13, 183, 26, 190, false);
    c.fillRect(28, 185, 29, 188);
    const int fill = (battery * 12 + 50) / 100;
    if (fill > 0) c.fillRect(14, 184, 13 + fill, 189);

    char label[24];
    if (gamesLeft < 0) snprintf(label, sizeof label, "%d%% charged", battery);
    else if (gamesLeft == 0) snprintf(label, sizeof label, "Charge soon");
    else if (gamesLeft == 1) snprintf(label, sizeof label, "~1 game left");
    else snprintf(label, sizeof label, "~%d games left", gamesLeft);
    c.text(kFontMenuDetail, 36, 179, label);
}

// ------------------------------------------------------------------ start-up modes
namespace {

constexpr int WIDE_MID = 100;                         // times split in competition and coach page 1
constexpr int COACH_GX[5] = {12, 59, 106, 153, 200};  // coach position columns
const char* const kPositions[4] = {"Lead", "2nd", "3rd", "Skip"};
const char* const kPositionShort[4] = {"L", "2", "3", "S"};

void rectOutline(Canvas& c, int x0, int y0, int x1, int y1, bool black = true) {
    c.fillRect(x0, y0, x1, y0, black);
    c.fillRect(x0, y1, x1, y1, black);
    c.fillRect(x0, y0, x0, y1, black);
    c.fillRect(x1, y0, x1, y1, black);
}

// The focus: a 2 px border round the half the rocker works on.
void focusBox(Canvas& c, int x0, int y0, int x1, int y1) {
    rectOutline(c, x0, y0, x1, y1);
    rectOutline(c, x0 + 1, y0 + 1, x1 - 1, y1 - 1);
}

// H2H times are a digit wider, so the H2H half of the large row is wider (x 92).
constexpr int COMP_MID = 92;
// No labels: B2H is black on white, H2H white on black, as in the columns.
constexpr int kTimesY1 = 44;  // the newest stone large in y 0-44 (2 px line under it)
constexpr int GRID_LABEL_Y0 = 46, GRID_LABEL_Y1 = 57;
constexpr int kLinesTop = 59;   // the columns' first line
constexpr int kCoachTimesY0 = 4;  // coach: the newest stone's digits start under the page marks (y 1-4)

// The current stone across both halves, centred: B2H black on white, H2H white on its black half
// (filled by the caller); longFont for an H2H of 20.00 s, too wide for font. A stopwatch where an
// awaited time will appear. Awaiting the B2H, or the H2H of a stone with no B2H, the row is still
// empty (no time yet).
void newestRow(Canvas& c, const Stone* stones, int count, Waiting waiting, int mid, const Font& font,
               const Font& longFont, int y0, int y1, const Sprite& watch, int cy) {
    const bool fresh = waiting == Waiting::BackToHog || waiting == Waiting::HogToHogOnly;  // no time yet
    const Stone* cur = fresh || count == 0 ? nullptr : &stones[0];
    const int32_t vals[2] = {cur ? cur->bh : -1, cur ? cur->hh : -1};
    const Waiting want[2] = {Waiting::BackToHog, waiting == Waiting::HogToHogOnly ? waiting : Waiting::HogToHog};
    for (int k = 0; k < 2; ++k) {
        const int x0 = k == 0 ? 0 : mid, x1 = k == 0 ? mid : W;
        const bool black = k == 0;
        if (vals[k] >= 0) dsegNumber(c, vals[k], x0, x1, y0, y1, 0, vals[k] >= 2000 ? longFont : font, true, black);
        else if (waiting == want[k]) c.blit(watch.bm, (x0 + x1) / 2 - watch.cx, cy - watch.cy, black);
    }
}

// Two small squares at the top right, white on the H2H half's black: filled = the page showing.
void pageMarks(Canvas& c, int page) {
    for (int k = 0; k < 2; ++k) {
        const int x = W - 12 + k * 6;
        if (k == page) c.fillRect(x, 1, x + 3, 4, false);
        else rectOutline(c, x, 1, x + 3, 4, false);
    }
}

// Coach-mode rock i of the game: empty, made or missed; bold ring = cursor.
void coachRock(Canvas& c, const ShotLog& shots, int i, int cx, int cy) {
    c.sprite(kRockSmall, cx, cy);
    if (i < shots.count()) c.sprite(shots.made(i) ? kMadeSmall : kMissedSmall, cx, cy);
    if (i == shots.cursor()) c.sprite(kCursorSmall, cx, cy);
}

void endRow(Canvas& c, const ShotLog& shots, int end, int cy) {
    char n[3];
    snprintf(n, sizeof n, "%d", end + 1);
    kEndFont.draw(c, n, 3, cy - 4);
    for (int p = 0; p < 4; ++p)
        for (int r = 0; r < 2; ++r) coachRock(c, shots, end * 8 + p * 2 + r, COACH_GX[p] + 15 + r * 17, cy);
}

}  // namespace

constexpr int kHintTop = kTimesY1 + 2;      // the hold hints: over the times

// A bordered box (2 px) centred in top..bottom, its lines centred, bold (drawn twice, a pixel apart)
// in Spleen 12x24.
static void tooltip(Canvas& c, const char* const* lines, int n, int top, int bottom) {
    const Font& f = kFontSpleen12x24;
    constexpr int cw = 12, lh = 24;
    int longest = 0;
    for (int i = 0; i < n; ++i) {
        const int len = static_cast<int>(strlen(lines[i]));
        if (len > longest) longest = len;
    }
    const int w = cw * longest + 1 + 16 < W - 12 ? cw * longest + 1 + 16 : W - 12, h = lh * n + 10;
    const int x0 = (W - w) / 2;
    const int y0 = top + static_cast<int>(floor((bottom - top - h) / 2.0));  // floors like Python's //
    const int y1 = y0 + h - 1;
    c.fillRect(x0, y0, x0 + w - 1, y1, false);
    rectOutline(c, x0, y0, x0 + w - 1, y1);
    rectOutline(c, x0 + 1, y0 + 1, x0 + w - 2, y1 - 1);
    int y = y0 + 5;
    for (int i = 0; i < n; ++i) {
        const int x = (W - (cw * static_cast<int>(strlen(lines[i])) + 1)) / 2;
        for (int dx = 0; dx < 2; ++dx) c.text(f, x + dx, y, lines[i]);
        y += lh;
    }
}

// Cover just the player history. The latest times and the lower strip stay visible.
static void switchOnTip(Canvas& c, uint8_t me) {
    c.fillRect(0, kHintTop, W - 1, kHintTop + kPositionCard.h - 1, false);
    c.blit(kPositionCard, 0, kHintTop);
    c.blit(kPositionTitles[me % 4], 8, kHintTop + 28);
}

// While the rocker centre is held, before the undo (1 s): the hold does something. tip says which:
// Undo, or PowerOff (the side button held, before it switches off).
static void undoTip(Canvas& c, int top, int bottom, Tip tip = Tip::Undo) {
    const char* const lines[2] = {"Keep holding", tip == Tip::PowerOff ? "to turn off" : "to undo"};
    tooltip(c, lines, 2, top, bottom);
}

// Coach: the rocker centre clicked once (switching page takes a double click).
static void pageTip(Canvas& c, int top, int bottom) {
    const char* const lines[2] = {"Double click", "to change page"};
    tooltip(c, lines, 2, top, bottom);
}

// While the rocker is held to move the newest time: one line, over the bottom (top..bottom), clear of
// the columns.
static void moveTip(Canvas& c, int top, int bottom) {
    const char* const lines[1] = {"Moving time"};
    tooltip(c, lines, 1, top, bottom);
}

// The top of the times, every mode: the newest stone large, B2H on white left of mid, H2H on black
// right of it (no labels: as in the columns), a stopwatch where an awaited time will appear.
// y0: the digits' box starts lower (coach: under the page marks).
static void timesTop(Canvas& c, const Stone* stones, int count, Waiting waiting, int mid, int y0 = 0) {
    c.fillRect(mid, 0, W - 1, kTimesY1 - 1);
    newestRow(c, stones, count, waiting, mid, kFontDseg37, kFontDseg33, y0, kTimesY1, kStopwatch, (y0 + kTimesY1) / 2);
    hline(c, 0, W - 1, kTimesY1, 2);
}

// The bottom TIP_H px, white on black, for an umpire: the logo on the left, the mode's name and
// "STOPWATCH ONLY" (or "Battery low", or a note) right-aligned.
static void badgeStrip(Canvas& c, const char* name, bool lowBattery, const char* note = nullptr) {
    const int strip = H - TIP_H;
    c.fillRect(0, strip, W - 1, H - 1);
    c.blit(kLogoStrip, 2, strip + (TIP_H - kLogoStrip.h) / 2, false);
    const char* const lines[2] = {name, note ? note : lowBattery ? "Battery low" : "STOPWATCH ONLY"};
    for (int i = 0; i < 2; ++i)
        c.text(kFontSpleen6x12, W - 2 - 6 * static_cast<int>(strlen(lines[i])), strip + 1 + 12 * i, lines[i], false);
}

// Basic: competition's times with no players, as big as they go: a row a stone, newest first, 4 rows,
// B2H black on white left of COMP_MID, H2H white on black right of it, the badge strip below. The rows
// scroll (scroll: rows down, to older stones); scrolled, the badge says which stones show. A stone
// being timed with no time yet has a row of its own at the top, with the stopwatch; one awaiting its
// H2H has the stopwatch in its H2H cell.
void drawBasic(Canvas& c, const Stone* stones, uint8_t count, bool lowBattery, Tip tip, Waiting waiting,
               int scroll) {
    c.clear();
    if (tip == Tip::BasicIntro) {
        c.blit(kBasicIntro, 0, 0);
        return;
    }
    const int strip = H - TIP_H, rowH = (H - TIP_H) / 4, kRows = 4;
    c.fillRect(COMP_MID, 0, W - 1, strip - 1);
    const bool fresh = waiting == Waiting::BackToHog || waiting == Waiting::HogToHogOnly;  // a row of its own
    const int rows = count + (fresh ? 1 : 0);
    if (scroll > rows - kRows) scroll = rows - kRows;
    if (scroll < 0) scroll = 0;
    int first = 0, last = 0;  // the stones shown, counting from the newest (1)
    char buf[12];
    for (int i = 0; i < kRows && scroll + i < rows; ++i) {
        const int r = scroll + i, s = r - (fresh ? 1 : 0);  // s < 0: the stone under way, no time yet
        const int y0 = i * rowH;
        if (i) {
            c.fillRect(0, y0, COMP_MID - 1, y0);  // between rows: black on white, white on black
            c.fillRect(COMP_MID, y0, W - 1, y0, false);
        }
        const int32_t vals[2] = {s < 0 ? -1 : stones[s].bh, s < 0 ? -1 : stones[s].hh};
        const bool watch[2] = {s < 0 && waiting == Waiting::BackToHog,
                               (s < 0 && waiting == Waiting::HogToHogOnly) || (s == 0 && waiting == Waiting::HogToHog)};
        for (int k = 0; k < 2; ++k) {
            const int x0 = k ? COMP_MID : 0, x1 = k ? W : COMP_MID;
            if (vals[k] >= 0)
                dsegNumber(c, vals[k], x0, x1, y0, y0 + rowH, 0, vals[k] >= 2000 ? kFontDseg33 : kFontDseg37, true, k == 0);
            else if (watch[k])
                c.blit(kStopwatch.bm, (x0 + x1) / 2 - kStopwatch.cx, y0 + rowH / 2 - kStopwatch.cy, k == 0);
        }
        if (s >= 0) {
            if (!first) first = s + 1;
            last = s + 1;
        }
    }
    if (scroll > 0) snprintf(buf, sizeof buf, "%d-%d of %d", first, last, count);
    badgeStrip(c, "BASIC", lowBattery, scroll > 0 ? buf : nullptr);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, kHintTop, strip, tip);
}

constexpr int kHistorySplits = 4;
constexpr int kHistoryMargin = 4;
constexpr int kHistoryBarHeight = 3;
constexpr int kThemLinesTop = 67;

static int columnScrollLimit(const Stone* stones, int count, uint8_t player) {
    int splits = 0;
    for (int i = 0; i < count; ++i) {
        if (stones[i].player == player) splits += (stones[i].bh >= 0) + (stones[i].hh >= 0);
    }
    return splits > kHistorySplits ? splits - kHistorySplits : 0;
}

int historyScrollLimit(const Stone* stones, uint8_t count, uint8_t me, uint8_t firstColumn) {
    int limit = 0;
    for (uint8_t slot = 0; slot < kVisibleTimeColumns; ++slot) {
        const uint8_t player = columnPlayer(me, firstColumn + slot);
        const int columnLimit = columnScrollLimit(stones, count, player);
        if (columnLimit > limit) limit = columnLimit;
    }
    return limit;
}

static void timingColumnBar(Canvas& c, uint8_t firstColumn, uint8_t columns, int top) {
    constexpr int width = W - 2 * kHistoryMargin;
    for (int column = 0; column < columns; ++column) {
        const int left = kHistoryMargin + column * width / columns;
        const int right = kHistoryMargin + (column + 1) * width / columns - 3;
        if (column >= firstColumn && column < firstColumn + kVisibleTimeColumns)
            c.fillRect(left, top, right, top + kHistoryBarHeight - 1);
        else
            hline(c, left, right, top + 1);
    }
}

static void drawTimingHistory(Canvas& c, const Stone* stones, uint8_t count, uint8_t me,
                              uint8_t active, TimeHistoryView history, int end, bool framed = false) {
    constexpr int columnWidth = (W - 2 * kHistoryMargin) / kVisibleTimeColumns;
    const uint8_t total = columnCount(me);
    const uint8_t firstColumn = firstVisibleTimeColumn(history.first, active, total);
    const int bottom = end - kHistoryBarHeight;
    int scroll = history.scrolling ? history.offset : 0;
    if (history.scrolling) {
        const int limit = historyScrollLimit(stones, count, me, firstColumn);
        if (scroll > limit) scroll = limit;
        const int trackTop = kLinesTop + 2, trackBottom = bottom - 4;
        const int track = framed ? 3 : W - 2;
        if (limit > 0) {
            constexpr int thumbHeight = 10;
            const int thumbTop = trackTop + scroll * (trackBottom - trackTop + 1 - thumbHeight) / limit;
            vline(c, track, trackTop, trackBottom);
            c.fillRect(track - 1, thumbTop, framed ? track : track + 1, thumbTop + thumbHeight - 1);
        } else if (framed) {
            c.fillRect(track - 1, trackTop, track, trackBottom);
        }
    }

    for (uint8_t slot = 0; slot < kVisibleTimeColumns; ++slot) {
        const uint8_t column = firstColumn + slot;
        const int left = kHistoryMargin + slot * columnWidth;
        const int right = left + columnWidth;
        const uint8_t player = columnPlayer(me, column);
        const bool theirs = player == kThem;
        const bool focused = column == active;
        if (theirs) {
            if (focused) c.fillRect(left, GRID_LABEL_Y0, W - 1, kThemLinesTop - 1);
            textInBox(c, kFontSpleen16x32, "THEM", left, W, GRID_LABEL_Y0, kThemLinesTop,
                      kCentre, 0, !focused);
        } else {
            const char* name = kPositions[player];
            if (focused) c.fillRect(left, GRID_LABEL_Y0, right - 1, GRID_LABEL_Y1);
            c.text(kFontSpleen6x12, (left + right - 6 * static_cast<int>(strlen(name))) / 2,
                   GRID_LABEL_Y0, name, !focused);
        }
        if (slot > 0) vline(c, left, GRID_LABEL_Y0, bottom - 1);

        const int linesTop = theirs ? kThemLinesTop : kLinesTop;
        const int pitch = (bottom - linesTop) / kHistorySplits;
        const int maxHeight = theirs ? 23 : 24;
        const int height = pitch - 3 < maxHeight ? pitch - 3 : maxHeight;
        const SegFont digits{height, 14, 3, 2, 3, true, true};
        const SegFont longDigits{height, 11, 3, 2, 3, true, true};
        const int limit = scroll > 0 ? columnScrollLimit(stones, count, player) : 0;
        int skip = scroll < limit ? scroll : limit;
        int row = 0;
        for (int i = 0; i < count && row < kHistorySplits; ++i) {
            if (stones[i].player != player) continue;
            const int32_t values[] = {stones[i].bh, stones[i].hh};
            for (int half = 0; half < 2 && row < kHistorySplits; ++half) {
                const int32_t value = values[half];
                if (value < 0) continue;
                if (skip > 0) { --skip; continue; }
                const int y = linesTop + row++ * pitch;
                if (half == 1) c.fillRect(left + 2, y, right - 2, y + pitch - 2);
                char label[12];
                formatTime(value, label, sizeof label);
                const SegFont& font = digits.width(label) <= columnWidth - 6 ? digits : longDigits;
                segInBox(c, font, label, left + 1, right - 1, y, y + pitch - 1, kCentre, 0, half == 0);
            }
        }
    }
    timingColumnBar(c, firstColumn, total, bottom);
}

void drawCompetition(Canvas& c, const Stone* stones, uint8_t count, uint8_t me, uint8_t activeCol,
                     bool lowBattery, Tip tip, Waiting waiting, TimeHistoryView history) {
    c.clear();
    const int strip = H - TIP_H;
    timesTop(c, stones, count, waiting, COMP_MID);
    drawTimingHistory(c, stones, count, me, activeCol, history, strip);
    badgeStrip(c, "COMPETITION", lowBattery, history.scrolling ? "SCROLL MODE" : nullptr);
    c.fillRect(0, strip, W - 1, strip, false);
    if (tip == Tip::SwitchOn) switchOnTip(c, me);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, kHintTop, strip, tip);
    if (tip == Tip::Move) moveTip(c, strip, H);  // over the badge strip
}

void drawLeague(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots, uint8_t me, uint8_t activeCol,
                bool lowBattery, Tip tip, Waiting waiting, Focus focus, TimeHistoryView history) {
    c.clear();
    const int top = 159;    // 2 px line under the times
    const int split = 149;  // 2 px divider left of the percentage: in line with the THEM column's
    timesTop(c, stones, count, waiting, COMP_MID);
    drawTimingHistory(c, stones, count, me, activeCol, history, top - 1, true);
    hline(c, 0, W - 1, top);  // 1 px like the columns: only the focus border is bold
    vline(c, split, top, H - 1);
    // your rocks like a scoreboard: ends across, rock 1 over rock 2
    const int cursor = shots.cursor();
    for (int end = 0; end < 8; ++end) {
        const int cx = 12 + end * 18;  // spread across the box
        char n[3];
        snprintf(n, sizeof n, "%d", end + 1);
        kEndFont.draw(c, n, cx - 2, top + 3);
        for (int r = 0; r < 2; ++r) {
            const int i = end * 2 + r, cy = top + 19 + r * 13;
            c.sprite(kRockSmall, cx, cy);
            if (i < shots.count()) c.sprite(shots.made(i) ? kMadeSmall : kMissedSmall, cx, cy);
            if (i == cursor) c.sprite(kCursorSmall, cx, cy);
        }
    }
    // the focus: a bold border around the half the rocker works on
    if (focus == Focus::Players) focusBox(c, 0, kTimesY1, W - 1, top + 1);
    else focusBox(c, 0, top, W - 1, H - 1);  // the whole bottom: your rocks and your %
    const int x0 = split + 2;
    int y0 = top + 2;
    if (lowBattery) {  // a black bar over the percentage
        c.fillRect(x0, y0, W - 3, y0 + 12);  // inside the focus border (x 198-199), focused or not
        textInBox(c, kFontSpleen6x12, "Low bat", x0, W - 2, y0, y0 + 12, kCentre, 4, false);
        y0 += 13;
    }
    if (shots.count() > 0) bigPercent(c, shots.percent(), x0, W - 2, y0, H);
    if (tip == Tip::SwitchOn) switchOnTip(c, me);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, kHintTop, top, tip);
    if (tip == Tip::Move) moveTip(c, top, H);  // over your rocks
}

void drawCoachTimer(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots, Waiting waiting,
                    uint8_t activeCol, Tip tip, Focus focus, TimeHistoryView history) {
    c.clear();
    const Font& label = kFontSpleen6x12;
    const int bottom = 150;
    timesTop(c, stones, count, waiting, WIDE_MID, kCoachTimesY0);  // the digits under the page marks
    pageMarks(c, 0);
    drawTimingHistory(c, stones, count, kCoachSeat, activeCol, history, bottom - 1, true);
    hline(c, 0, W - 1, bottom, 2);
    for (int p = 0; p < 4; ++p) {
        if (p) vline(c, COACH_GX[p], bottom, H - 1);
        const int v = shots.positionPercent(p);
        char text[8];
        if (v >= 0) snprintf(text, sizeof text, "%s %d%%", kPositionShort[p], v);
        else snprintf(text, sizeof text, "%s", kPositionShort[p]);
        textInBox(c, label, text, COACH_GX[p], COACH_GX[p + 1], bottom + 2, bottom + 14, kCentre, 4);
    }
    // the previous end and the one under way
    const int slots = shots.capacity();
    int current = (shots.count() < slots - 1 ? shots.count() : slots - 1) / 8;
    int first = current - 1;
    if (first > 6) first = 6;
    if (first < 0) first = 0;
    for (int row = 0; row < 2; ++row) endRow(c, shots, first + row, bottom + 24 + row * 15);
    if (focus == Focus::Players) focusBox(c, 0, kTimesY1, W - 1, bottom + 1);
    else focusBox(c, 0, bottom, W - 1, H - 1);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, kHintTop, bottom, tip);
    if (tip == Tip::Page) pageTip(c, kHintTop, bottom);
    if (tip == Tip::Move) moveTip(c, bottom, H);  // over the rocks
}

void drawCoachRocks(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots, Waiting waiting,
                    Tip tip, Focus focus) {
    c.clear();
    const Font& label = kFontSpleen6x12;
    c.fillRect(WIDE_MID, 0, W - 1, 32);  // H2H white on black, as on page 1
    newestRow(c, stones, count, waiting, WIDE_MID, kFontDseg24, kFontDseg24, kCoachTimesY0, 33, kStopwatchSmall,
              (kCoachTimesY0 + 33) / 2);
    pageMarks(c, 1);
    hline(c, 0, W - 1, 33, 2);
    for (int p = 0; p < 4; ++p) {
        textInBox(c, label, kPositions[p], COACH_GX[p], COACH_GX[p + 1], 35, 47, kCentre, 4);
        if (p) vline(c, COACH_GX[p], 35, 185);
    }
    hline(c, 0, W - 1, 47);
    for (int end = 0; end < 8; ++end) endRow(c, shots, end, 55 + end * 16);
    hline(c, 0, W - 1, 185, 2);
    for (int p = 0; p < 4; ++p) {
        if (p) vline(c, COACH_GX[p], 185, H - 1);
        const int v = shots.positionPercent(p);
        if (v < 0) continue;
        char text[6];
        snprintf(text, sizeof text, "%d%%", v);
        const int w = 13 + 6 * static_cast<int>(strlen(text));
        const int x = (COACH_GX[p] + COACH_GX[p + 1] - w) / 2;  // both positive: / floors like //
        drawPie(c, x + 5, 193, v);
        c.text(label, x + 13, 187, text);
    }
    if (focus == Focus::Players) focusBox(c, 0, 0, W - 1, 34);
    else focusBox(c, 0, 33, W - 1, 186);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, 47, 185, tip);  // over the rocks
    if (tip == Tip::Page) pageTip(c, 47, 185);
    if (tip == Tip::Move) moveTip(c, 150, H);
}

void addStone(Stone* list, uint8_t& count, uint8_t capacity, const Stone& s) {
    if (count < capacity) {
        memmove(&list[1], &list[0], sizeof(Stone) * count);
        list[0] = s;
        ++count;
        return;
    }
    const Stone dropped = list[capacity - 1];  // the oldest: pushed off the end
    memmove(&list[1], &list[0], sizeof(Stone) * (capacity - 1));
    list[0] = s;
    auto group = [](const Stone& s) { return s.player <= Stone::kThem ? s.player : 5; };  // 5: no player
    int per[6] = {};
    for (int i = 0; i < capacity; ++i) ++per[group(list[i])];
    if (group(dropped) == 5 || per[group(dropped)] >= kGridRows) return;  // nothing to keep it for
    for (int i = capacity - 1; i > 0; --i) {
        if (per[group(list[i])] > kGridRows) {
            // drop this one instead, and keep the other as the oldest
            memmove(&list[i], &list[i + 1], sizeof(Stone) * (capacity - 1 - i));
            list[capacity - 1] = dropped;
            return;
        }
    }
}

}  // namespace stoprock
