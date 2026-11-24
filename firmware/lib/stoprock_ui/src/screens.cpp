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

constexpr SegFont kRowFont{12, 7, 2, 2, 2, false};  // older results
constexpr SegFont kEndFont{9, 5, 1, 1, 1, false};   // end numbers: equal-width cells, a "1" sits on the right
constexpr SegFont kNarrowFont{12, 6, 2, 1, 2, true};  // coach's five columns: a narrow "1"
// The cursor's column, zoomed (playerColumns()): the biggest digits 4 lines fit in each mode's column
// height and 1.3 columns' width (a white-on-black cell keeping 1 px of black round them), a narrow
// solid "1"; longFont: narrower, for 20.00, the one time with no "1" to spare.
struct Zoom { SegFont font, longFont; };
constexpr Zoom kZoomLeague{{22, 13, 3, 2, 3, true, true}, {22, 11, 3, 2, 3, true, true}};
constexpr Zoom kZoomComp{{24, 14, 3, 2, 3, true, true}, {24, 11, 3, 2, 3, true, true}};
constexpr Zoom kZoomCoach{{18, 11, 2, 2, 2, true, true}, {18, 8, 2, 2, 2, true, true}};
constexpr int kZoomLines = 4;      // filling the column's height
constexpr double kZoomSpread = 1.3;  // its width, in columns: over 15% of each neighbour (30% of one at an edge)

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

void drawHelp(Canvas& c, const char* lastGame, int shotPct, int battery, const char* batteryNote) {
    c.clear();
    const Font& f = kFontSpleen6x12;
    c.blit(kLogo, (W - kLogo.w) / 2, 4);
    int y = 4 + kLogo.h + 6;

    auto tag = [&](const char* labelText, const char* rest) {
        const int w = static_cast<int>(strlen(labelText)) * 6;
        c.fillRect(3, y, 4 + w + 2, y + 12);
        c.text(f, 5, y + 1, labelText, false);
        c.text(f, 5 + w + 8, y + 1, rest);
        y += 14;
    };

    // every "=" on the page sits in one column, where SHOTS' "up/down = " puts it
    const int eqX = 5 + 6 * 5 + 8 + 6 * 8;
    auto eqLine = [&](const char* s) {
        const char* eq = strchr(s, '=');
        c.text(f, eqX - 6 * static_cast<int>(eq ? eq - s : 0), y, s);
        y += 12;
    };
    tag("START", "hold side button, plus:");
    eqLine("nothing     = mode menu");
    eqLine("rocker down = league mode");
    eqLine("rocker up   = coach mode");
    y += 5;
    tag("TIMING", "use top or side button");
    eqLine("hold up = move last time");
    y += 3;
    tag("SHOTS", "up/down = made/missed");
    y += 3;
    tag("UNDO", "hold centre: time or shot");
    y += 4;
    c.line(4, y, 195, y);
    y += 5;
    // Bottom right: shot percentage with a pie, battery with its icon. The icons end at the
    // same x (room for "100%" after them), and the percentages are right-aligned so the
    // % signs line up. That leaves 26 characters for "Last game 16/16 shots made".
    const int iconRight = W - 29, pctRight = W - 2;
    auto percent = [&](int value) {
        char pct[6];
        snprintf(pct, sizeof pct, "%d%%", value < 0 ? 0 : value > 100 ? 100 : value);
        c.text(f, pctRight - 6 * static_cast<int>(strlen(pct)), y, pct);
    };
    c.text(f, 4, y, lastGame);
    if (shotPct >= 0) {
        drawPie(c, iconRight - 5, y + 6, shotPct);
        percent(shotPct);
    }
    y += 12;
    if (batteryNote && *batteryNote) c.text(f, 4, y, batteryNote);  // games left, from past drain
    if (battery >= 0) {
        const int bx = iconRight - 14;
        c.fillRect(bx, y + 3, bx + 12, y + 3);       // outline
        c.fillRect(bx, y + 9, bx + 12, y + 9);
        c.fillRect(bx, y + 3, bx, y + 9);
        c.fillRect(bx + 12, y + 3, bx + 12, y + 9);
        c.fillRect(bx + 13, y + 5, bx + 14, y + 7);  // terminal
        const int fill = pyRound(9.0 * (battery > 100 ? 100 : battery) / 100);
        if (fill > 0) c.fillRect(bx + 2, y + 5, bx + 2 + fill - 1, y + 7);
        percent(battery);
        y += 12;
    }
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

// Competition and league history: four columns, your teammates and THEM (leagueTimes). H2H times
// are a digit wider, so the H2H half of the large row is wider (x 92).
constexpr int COMP_MID = 92;
// No labels: B2H is black on white, H2H white on black, as in the columns.
constexpr int kTimesY1 = 44;  // the newest stone large in y 0-44 (2 px line under it)
constexpr int GRID_LABEL_Y0 = 46, GRID_LABEL_Y1 = 57;
constexpr int GRID_PITCH = 14;  // a line of the other columns' times
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
// in Spleen 12x24, or 16x32 if big.
static void tooltip(Canvas& c, const char* const* lines, int n, int top, int bottom, bool big = false) {
    const Font& f = big ? kFontSpleen16x32 : kFontSpleen12x24;
    const int cw = big ? 16 : 12, lh = big ? 32 : 24;
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

// The switch-on tip (competition and league), also shown when you change position: "You are / SKIP"
// (bold Spleen 16x32) and "2x to change" (12x24) in a box on the left, level with the rocker, and an
// arrow from it to the rocker (a double click changes position).
constexpr int kRockerY = 59;  // the rocker's height beside the screen (CoreInk: right side)
static const char* const kYouAre[4] = {"LEAD", "SECOND", "THIRD", "SKIP"};

// An arrow from xFrom to the screen's right edge at y, pointing right, on a white band (so it reads
// over anything): a 5 px shaft and a head 9 px each side, drawn in columns.
static void arrowRight(Canvas& c, int xFrom, int y) {
    constexpr int tip = W - 1, head = 9;
    c.fillRect(xFrom, y - head - 2, tip, y + head + 2, false);
    c.fillRect(xFrom, y - 2, tip - head, y + 2);
    for (int i = 0; i <= head; ++i) c.fillRect(tip - i, y - i, tip - i, y + i);
}

static void switchOnTip(Canvas& c, uint8_t me) {
    struct Line { const char* text; bool big; };
    const Line lines[3] = {{"You are", true}, {kYouAre[me % 4], true}, {"2x to change", false}};
    int w = 0, h = 10;
    for (const Line& l : lines) {
        const int lw = (l.big ? 16 : 12) * static_cast<int>(strlen(l.text));
        if (lw > w) w = lw;
        h += l.big ? 32 : 24;
    }
    w += 1 + 16;
    const int x0 = 2, y0 = kRockerY - 11, x1 = x0 + w - 1, y1 = y0 + h - 1;
    c.fillRect(x0, y0, x1, y1, false);
    rectOutline(c, x0, y0, x1, y1);
    rectOutline(c, x0 + 1, y0 + 1, x1 - 1, y1 - 1);
    int y = y0 + 5;
    for (const Line& l : lines) {
        const int cw = l.big ? 16 : 12;
        const int x = x0 + (w - (cw * static_cast<int>(strlen(l.text)) + 1)) / 2;
        for (int dx = 0; dx < 2; ++dx) c.text(l.big ? kFontSpleen16x32 : kFontSpleen12x24, x + dx, y, l.text);
        y += l.big ? 32 : 24;
    }
    arrowRight(c, x1 + 1, kRockerY);
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

// The zoomed column's edges (za, zb): kZoomSpread columns wide, centred on the cursor's column, kept
// inside x0..x1 (at an edge it takes all the extra from its one neighbour).
static void zoomSpan(int x0, int x1, int n, int active, int& za, int& zb) {
    const int zw = pyRound(kZoomSpread * (x1 - x0) / n);
    za = pyRound(x0 + static_cast<double>((x1 - x0) * (2 * active + 1)) / (2 * n) - zw / 2.0);
    if (za > x1 - zw) za = x1 - zw;
    if (za < x0) za = x0;
    zb = za + zw;
}

// A column per player between x0 and x1 (n columns: your teammates or, for a coach (me kCoachSeat),
// every position, then THEM), equal widths, headed with the position or THEM. The cursor's (active)
// is zoomed, drawn over its neighbours (zoomSpan): its header highlighted, its newest 4 times in the
// biggest digits that fit, filling the column's height. The others list theirs newest first, a time
// a line, in the part the zoom leaves them (narrower digits when it leaves too little). B2H black on
// white, H2H white on black.
static void playerColumns(Canvas& c, const Stone* stones, int count, uint8_t me, int x0, int x1, int n,
                          uint8_t active, int bottom, const SegFont& font, const Zoom& zoom) {
    int xs[kCoachColumns + 1];
    for (int k = 0; k <= n; ++k) xs[k] = pyRound(x0 + static_cast<double>((x1 - x0) * k) / n);
    int za, zb;
    zoomSpan(x0, x1, n, active, za, zb);
    auto nameOf = [&](int k) {
        const uint8_t player = columnPlayer(me, static_cast<uint8_t>(k));
        return player == kThem ? "THEM" : kPositions[player];
    };
    // column k's times, newest first, up to most: each a value and whether it's an H2H
    char buf[12];
    auto times = [&](int k, int most, int32_t* vals, bool* hh) {
        int shown = 0;
        for (int i = 0; i < count && shown < most; ++i) {
            if (playerColumn(me, stones[i].player) != k) continue;
            const int32_t v[2] = {stones[i].bh, stones[i].hh};
            for (int half = 0; half < 2 && shown < most; ++half) {
                if (v[half] < 0) continue;
                vals[shown] = v[half];
                hh[shown++] = half == 1;
            }
        }
        return shown;
    };
    auto cell = [&](const SegFont& cf, int32_t val, bool inverted, int a, int b, int y, int pitch) {
        if (inverted) c.fillRect(a + 1, y, b - 1, y + pitch - 2);
        formatTime(val, buf, sizeof buf);
        segInBox(c, cf, buf, a, b, y, y + pitch - 1, kCentre, 0, !inverted);
    };
    constexpr int kMostLines = 16;
    int32_t vals[kMostLines];
    bool hh[kMostLines];
    int most = (bottom - kLinesTop) / GRID_PITCH;
    if (most > kMostLines) most = kMostLines;
    for (int k = 0; k < n; ++k) {
        if (k == active) continue;
        const int a = k > active && zb > xs[k] ? zb : xs[k];  // the part the zoom leaves
        const int b = k < active && za < xs[k + 1] ? za : xs[k + 1];
        if (k) vline(c, a, GRID_LABEL_Y0, bottom - 1);
        const char* name = nameOf(k);
        c.text(kFontSpleen6x12, (a + b - 6 * static_cast<int>(strlen(name))) / 2, GRID_LABEL_Y0, name);
        const int shown = times(k, most, vals, hh);
        int widest = 0;
        for (int i = 0; i < shown; ++i) {
            formatTime(vals[i], buf, sizeof buf);
            if (font.width(buf) > widest) widest = font.width(buf);
        }
        const SegFont& cf = widest + 4 <= b - a - 2 ? font : kNarrowFont;
        for (int i = 0; i < shown; ++i) cell(cf, vals[i], hh[i], a + 1, b - 1, kLinesTop + i * GRID_PITCH, GRID_PITCH);
    }
    // the zoom: lines at its edges (none at the outer ones), the header highlighted
    if (za > x0) vline(c, za, GRID_LABEL_Y0, bottom - 1);
    if (zb < x1) vline(c, zb, GRID_LABEL_Y0, bottom - 1);
    c.fillRect(za + (za > x0 ? 1 : 0), GRID_LABEL_Y0, zb - 1, GRID_LABEL_Y1);
    const char* name = nameOf(active);
    c.text(kFontSpleen6x12, (za + zb - 6 * static_cast<int>(strlen(name))) / 2, GRID_LABEL_Y0, name, false);
    const int pitch = (bottom - kLinesTop) / kZoomLines;
    const int shown = times(active, kZoomLines, vals, hh);
    for (int i = 0; i < shown; ++i)
        cell(vals[i] >= 2000 ? zoom.longFont : zoom.font, vals[i], hh[i], za + 1, zb - 1, kLinesTop + i * pitch, pitch);
}

static void leagueTimes(Canvas& c, const Stone* stones, int count, uint8_t me, uint8_t active, int bottom,
                        const Zoom& zoom) {
    playerColumns(c, stones, count, me, 2, 198, kLeagueColumns, active, bottom, kRowFont, zoom);
}

// The top of the times, every mode: the newest stone large, B2H on white left of mid, H2H on black
// right of it (no labels: as in the columns), a stopwatch where an awaited time will appear.
// y0: the digits' box starts lower (coach: under the page marks).
static void timesTop(Canvas& c, const Stone* stones, int count, Waiting waiting, int mid, int y0 = 0) {
    c.fillRect(mid, 0, W - 1, kTimesY1 - 1);
    newestRow(c, stones, count, waiting, mid, kFontDseg37, kFontDseg33, y0, kTimesY1, kStopwatch, (y0 + kTimesY1) / 2);
    hline(c, 0, W - 1, kTimesY1, 2);
}

// The newest stone large, then the four columns down to bottom.
static void fourColumns(Canvas& c, const Stone* stones, int count, uint8_t me, uint8_t active, int bottom,
                        Waiting waiting, const Zoom& zoom) {
    timesTop(c, stones, count, waiting, COMP_MID);
    leagueTimes(c, stones, count, me, active, bottom, zoom);
}

// The modes at switch-on, big (bold Spleen 16x32), one a line; the selected one white on black. No help text: the help
// page has it.
void drawModeMenu(Canvas& c, uint8_t selected) {
    c.clear();
    c.blit(kLogoStrip, (W - kLogoStrip.w) / 2, 4);
    static const char* const kModes[4] = {"COMPETITION", "LEAGUE", "COACH", "ZOOM"};
    constexpr int kRowTop = 30, kRowPitch = 42, kRowH = 38;
    for (int i = 0; i < 4; ++i) {
        const int y0 = kRowTop + i * kRowPitch;
        const bool on = i == selected;
        if (on) c.fillRect(2, y0, W - 3, y0 + kRowH - 1);
        const InkBox b = c.ink(kFontSpleen16x32, kModes[i]);
        const int x = (W - (b.r - b.l + 1)) / 2 - b.l, y = y0 + (kRowH - (b.b - b.t)) / 2 - b.t;
        for (int dx = 0; dx < 2; ++dx) c.text(kFontSpleen16x32, x + dx, y, kModes[i], !on);  // bold: twice, a pixel apart
    }
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

// Zoom: competition's times with no players, as big as they go: a row a stone, newest first, 4 rows,
// B2H black on white left of COMP_MID, H2H white on black right of it, the badge strip below. The rows
// scroll (scroll: rows down, to older stones); scrolled, the badge says which stones show. A stone
// being timed with no time yet has a row of its own at the top, with the stopwatch; one awaiting its
// H2H has the stopwatch in its H2H cell.
void drawZoom(Canvas& c, const Stone* stones, uint8_t count, bool lowBattery, Tip tip, Waiting waiting,
              int scroll) {
    c.clear();
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
    badgeStrip(c, "ZOOM", lowBattery, scroll > 0 ? buf : nullptr);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, kHintTop, strip, tip);
}

void drawCompetition(Canvas& c, const Stone* stones, uint8_t count, uint8_t me, uint8_t activeCol,
                     bool lowBattery, Tip tip, Waiting waiting) {
    c.clear();
    const int strip = H - TIP_H;
    fourColumns(c, stones, count, me, activeCol, strip, waiting, kZoomComp);
    badgeStrip(c, "COMPETITION", lowBattery);
    if (tip == Tip::SwitchOn) switchOnTip(c, me);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, kHintTop, strip, tip);
    if (tip == Tip::Move) moveTip(c, strip, H);  // over the badge strip
}

void drawLeague(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots, uint8_t me, uint8_t activeCol,
                bool lowBattery, Tip tip, Waiting waiting, Focus focus) {
    c.clear();
    const int top = 159;    // 2 px line under the times
    const int split = 149;  // 2 px divider left of the percentage: in line with the THEM column's
    fourColumns(c, stones, count, me, activeCol, top, waiting, kZoomLeague);
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
                    uint8_t activeCol, Tip tip, Focus focus) {
    c.clear();
    const Font& label = kFontSpleen6x12;
    const int bottom = 150;
    timesTop(c, stones, count, waiting, WIDE_MID, kCoachTimesY0);  // the digits under the page marks
    pageMarks(c, 0);
    playerColumns(c, stones, count, kCoachSeat, 0, W, kCoachColumns, activeCol, bottom, kNarrowFont, kZoomCoach);
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
