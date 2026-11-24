// Port of tools/mockups/screen.py. Keep the two in step: the mockups are the reference.
#include "screens.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace stoprock {
namespace {

constexpr int W = 200, H = 200;
constexpr int LABEL_H = 12, TIP_H = 26;  // TIP_H: competition's badge strip

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

// Right-aligned DSEG7 number with a small square decimal point.
void dsegNumber(Canvas& c, int32_t hundredths, double x0, double x1, double y0, double y1, double margin = 3,
                const Font& f = kFontDseg25) {
    char ip[12], fp[4];
    snprintf(ip, sizeof ip, "%ld", static_cast<long>(hundredths / 100));
    snprintf(fp, sizeof fp, "%02ld", static_cast<long>(hundredths % 100));
    InkBox bi = c.ink(f, ip), bf = c.ink(f, fp);
    const int t = bi.t < bf.t ? bi.t : bf.t, b = bi.b > bf.b ? bi.b : bf.b;
    const int dot = pyRound((b - t) * 0.13) > 3 ? pyRound((b - t) * 0.13) : 3;
    const int gap = 2;
    const double total = (bi.r - bi.l) + gap + dot + gap + (bf.r - bf.l);
    const double x = x1 - margin - total;
    const double y = (y0 + y1 - (b - t)) / 2.0 - t;
    c.text(f, pyRound(x - bi.l), pyRound(y), ip);
    const double xd = x + (bi.r - bi.l) + gap;
    const double yb = y + b;
    c.fillRect(pyRound(xd), pyRound(yb - dot), pyRound(xd + dot - 1), pyRound(yb - 1));
    c.text(f, pyRound(xd + dot + gap - bf.l), pyRound(y), fp);
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
    eqLine("nothing     = competition mode");
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

void rectOutline(Canvas& c, int x0, int y0, int x1, int y1) {
    c.fillRect(x0, y0, x1, y0);
    c.fillRect(x0, y1, x1, y1);
    c.fillRect(x0, y0, x0, y1);
    c.fillRect(x1, y0, x1, y1);
}

// The focus: a 2 px border round the half the rocker works on.
void focusBox(Canvas& c, int x0, int y0, int x1, int y1) {
    rectOutline(c, x0, y0, x1, y1);
    rectOutline(c, x0 + 1, y0 + 1, x1 - 1, y1 - 1);
}

// Competition and league history: four columns, your teammates and THEM (leagueTimes). H2H times
// are a digit wider, so the H2H half of the large row is wider (x 92).
constexpr int COMP_MID = 92;
constexpr int GRID_LABEL_Y0 = 49, GRID_LABEL_Y1 = 60;
constexpr int GRID_PITCH = 14;

// The current stone across both halves; a stopwatch where an awaited time will appear.
// Awaiting the B2H, or the H2H of a stone with no B2H, the row is still empty (no time yet).
void newestRow(Canvas& c, const Stone* stones, int count, Waiting waiting, int mid, const Font& font, int y0,
               int y1, int margin, const Sprite& watch, int cy) {
    const bool fresh = waiting == Waiting::BackToHog || waiting == Waiting::HogToHogOnly;  // no time yet
    const Stone* cur = fresh || count == 0 ? nullptr : &stones[0];
    const int32_t vals[2] = {cur ? cur->bh : -1, cur ? cur->hh : -1};
    const Waiting want[2] = {Waiting::BackToHog, waiting == Waiting::HogToHogOnly ? waiting : Waiting::HogToHog};
    for (int k = 0; k < 2; ++k) {
        const int x0 = k == 0 ? 0 : mid, x1 = k == 0 ? mid : W;
        if (vals[k] >= 0) dsegNumber(c, vals[k], x0, x1, y0, y1, margin, font);
        else if (waiting == want[k]) c.sprite(watch, (x0 + x1) / 2, cy);
    }
}

// Two small squares top right: filled = the page showing.
void pageMarks(Canvas& c, int page) {
    for (int k = 0; k < 2; ++k) {
        const int x = W - 16 + k * 8;
        if (k == page) c.fillRect(x, 3, x + 5, 8);
        else rectOutline(c, x, 3, x + 5, 8);
    }
}

// Coach-mode rock i of the game: empty, made or missed; bold ring = cursor.
void coachRock(Canvas& c, const ShotLog& shots, int i, int cx, int cy) {
    c.sprite(kRockSmall, cx, cy);
    if (i < shots.count()) c.sprite(shots.made(i) ? kCheckSmall : kCrossSmall, cx, cy);
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

// A bordered box centred in the times area (top..bottom), lines centred; an empty line is a
// half-line gap.
static void tooltip(Canvas& c, const char* const* lines, int n, int top, int bottom, bool headers = true) {
    int longest = 0, height = 0;
    for (int i = 0; i < n; ++i) {
        const int len = static_cast<int>(strlen(lines[i]));
        if (len > longest) longest = len;
        height += len ? 12 : 6;
    }
    const int w = 6 * longest + 16, h = height + 14;
    const int x0 = (W - w) / 2;
    int y0 = top + (bottom - top - h) / 2;
    if (headers && y0 < top + 12) y0 = top;  // it would cut through the column headers: cover them instead
    const int y1 = y0 + h - 1;
    c.fillRect(x0, y0, x0 + w - 1, y1, false);
    rectOutline(c, x0, y0, x0 + w - 1, y1);
    rectOutline(c, x0 + 1, y0 + 1, x0 + w - 2, y1 - 1);
    int y = y0 + 7;
    for (int i = 0; i < n; ++i) {
        const int len = static_cast<int>(strlen(lines[i]));
        if (len) c.text(kFontSpleen6x12, (W - 6 * len) / 2, y, lines[i]);
        y += len ? 12 : 6;
    }
}

// Switch-on tips: your position, how to change it, and what holding the rocker does (competition
// has no rocks, so only moving the last time). Also shown when you change position.
static const char* const kYouAre[4] = {"You are LEAD", "You are SECOND", "You are THIRD", "You are SKIP"};
// While the rocker centre is held, before the undo (1 s): the hold does something. tip says which:
// Undo, or PowerOff (the side button held, before it switches off).
static void undoTip(Canvas& c, int top, int bottom, Tip tip = Tip::Undo) {
    const char* const lines[1] = {tip == Tip::PowerOff ? "Keep holding to turn off" : "Keep holding to undo"};
    tooltip(c, lines, 1, top, bottom);
}

// While the rocker is held to move the newest time: over the bottom (top..bottom), clear of the columns.
static void moveTip(Canvas& c, int top, int bottom) {
    const char* const lines[1] = {"Keep holding to move time"};
    tooltip(c, lines, 1, top, bottom, false);
}

static void switchOnTip(Canvas& c, uint8_t me, bool league, int top, int bottom) {
    const char* lines[8] = {kYouAre[me % 4], "", "Double click rocker to", "change your position", "",
                            "Hold rocker down: your rocks", "Hold rocker up: move last time", "Hold rocker middle: undo"};
    if (!league) {  // competition: no rocks
        lines[5] = lines[6];
        lines[6] = lines[7];
    }
    tooltip(c, lines, league ? 8 : 7, top, bottom);
}

// Competition and league's four columns: your teammates in throwing order, then THEM. Each lists its stones newest
// first, a time a line: B2H black on white, H2H white on black.
// A column per player (edges xs; n columns: your teammates or, for a coach (me kCoach), every
// position, then THEM), the active one's header highlighted; each lists its stones newest first.
static void playerColumns(Canvas& c, const Stone* stones, int count, uint8_t me, const int* xs, int n,
                          uint8_t active, int bottom, int linesMax, const SegFont& font) {
    constexpr int kTop = 62;
    for (int k = 0; k < n; ++k) {
        const int x0 = xs[k], x1 = xs[k + 1];
        if (k) vline(c, x0, GRID_LABEL_Y0, bottom - 1);
        const bool on = k == active;
        if (on) c.fillRect(x0 + (k ? 1 : 0), GRID_LABEL_Y0, x1 - 1, GRID_LABEL_Y1);
        const uint8_t player = columnPlayer(me, k);
        const char* name = player == kThem ? "THEM" : kPositions[player];
        c.text(kFontSpleen6x12, (x0 + x1 - 6 * static_cast<int>(strlen(name))) / 2, GRID_LABEL_Y0, name, !on);
    }
    char buf[12];
    int lines[kCoachColumns] = {};
    for (int i = 0; i < count; ++i) {
        const int k = playerColumn(me, stones[i].player);
        if (k < 0 || k >= n) continue;
        const int x0 = xs[k] + 1, x1 = xs[k + 1] - 1;
        const int32_t vals[2] = {stones[i].bh, stones[i].hh};
        for (int half = 0; half < 2; ++half) {
            if (vals[half] < 0 || lines[k] >= linesMax) continue;
            const int y = kTop + lines[k]++ * GRID_PITCH;
            const bool inverted = half == 1;
            if (inverted) c.fillRect(x0 + 1, y, x1 - 1, y + GRID_PITCH - 2);
            formatTime(vals[half], buf, sizeof buf);
            segInBox(c, font, buf, x0, x1, y, y + GRID_PITCH - 1, kCentre, 0, !inverted);
        }
    }
}

static void leagueTimes(Canvas& c, const Stone* stones, int count, uint8_t me, uint8_t active, int bottom,
                        int linesMax) {
    static const int xs[kLeagueColumns + 1] = {2, 51, 100, 149, 198};
    playerColumns(c, stones, count, me, xs, kLeagueColumns, active, bottom, linesMax, kRowFont);
}

// The top of the times, every mode: Back2Hog | Hog2Hog labels (split at mid; the H2H label ends at
// labelRight), the newest stone large, a stopwatch where an awaited time will appear.
static void timesTop(Canvas& c, const Stone* stones, int count, Waiting waiting, int mid, int labelRight) {
    const Font& label = kFontSpleen6x12;
    textInBox(c, label, "Back2Hog", 0, mid, 0, LABEL_H, kCentre, 4);
    textInBox(c, label, "Hog2Hog", mid, labelRight, 0, LABEL_H, kCentre, 4);
    hline(c, 0, W - 1, LABEL_H);
    newestRow(c, stones, count, waiting, mid, kFontDseg25, 13, 47, 3, kStopwatch, 30);
    hline(c, 0, W - 1, 47, 2);
    vline(c, mid, 0, 46);
}

// The newest stone large, then the four columns down to bottom.
static void fourColumns(Canvas& c, const Stone* stones, int count, uint8_t me, uint8_t active, int bottom,
                        int linesMax, Waiting waiting) {
    timesTop(c, stones, count, waiting, COMP_MID, W);
    leagueTimes(c, stones, count, me, active, bottom, linesMax);
}

void drawCompetition(Canvas& c, const Stone* stones, uint8_t count, uint8_t me, uint8_t activeCol,
                     bool lowBattery, Tip tip, Waiting waiting) {
    c.clear();
    const Font& label = kFontSpleen6x12;
    const int strip = H - TIP_H;
    fourColumns(c, stones, count, me, activeCol, strip, kCompetitionRows, waiting);
    // the badge strip: the logo white on black, and two lines right-aligned
    c.fillRect(0, strip, W - 1, H - 1);
    c.blit(kLogoStrip, 2, strip + (TIP_H - kLogoStrip.h) / 2, false);
    const char* const lines[2] = {"COMPETITION", lowBattery ? "Battery low" : "STOPWATCH ONLY"};
    for (int i = 0; i < 2; ++i)
        c.text(label, W - 2 - 6 * static_cast<int>(strlen(lines[i])), strip + 1 + 12 * i, lines[i], false);
    if (tip == Tip::SwitchOn) switchOnTip(c, me, false, 49, strip);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, 49, strip, tip);
    if (tip == Tip::Move) moveTip(c, strip, H);  // over the badge strip
}

void drawLeague(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots, uint8_t me, uint8_t activeCol,
                bool lowBattery, Tip tip, Waiting waiting, Focus focus) {
    c.clear();
    const int top = 159;    // 2 px line under the times
    const int split = 134;  // 2 px divider left of the percentage
    fourColumns(c, stones, count, me, activeCol, top, kLeagueRows, waiting);
    hline(c, 0, W - 1, top, 2);
    vline(c, split, top, H - 1, 2);
    // your rocks like a scoreboard: ends across, rock 1 over rock 2
    const int cursor = shots.cursor();
    for (int end = 0; end < 8; ++end) {
        const int cx = 10 + end * 16;
        char n[3];
        snprintf(n, sizeof n, "%d", end + 1);
        kEndFont.draw(c, n, cx - 2, top + 3);
        for (int r = 0; r < 2; ++r) {
            const int i = end * 2 + r, cy = top + 19 + r * 13;
            c.sprite(kRockSmall, cx, cy);
            if (i < shots.count()) c.sprite(shots.made(i) ? kCheckSmall : kCrossSmall, cx, cy);
            if (i == cursor) c.sprite(kCursorSmall, cx, cy);
        }
    }
    // the focus: a bold border around the half the rocker works on
    if (focus == Focus::Players) focusBox(c, 0, 47, W - 1, top + 1);
    else focusBox(c, 0, top, split + 1, H - 1);
    const int x0 = split + 2;
    int y0 = top + 2;
    if (lowBattery) {  // a black bar over the percentage
        c.fillRect(x0, y0, W - 1, y0 + 12);
        textInBox(c, kFontSpleen6x12, "Batt low", x0, W, y0, y0 + 12, kCentre, 4, false);
        y0 += 13;
    }
    if (shots.count() > 0) bigPercent(c, shots.percent(), x0, W, y0, H);
    if (tip == Tip::SwitchOn) switchOnTip(c, me, true, 49, top);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, 49, top, tip);
    if (tip == Tip::Move) moveTip(c, top, H);  // over your rocks
}

void drawCoachTimer(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots, Waiting waiting,
                    uint8_t activeCol, Tip tip, Focus focus) {
    c.clear();
    const Font& label = kFontSpleen6x12;
    pageMarks(c, 0);
    const int bottom = 150;
    timesTop(c, stones, count, waiting, WIDE_MID, W - 18);  // left of the page marks
    static const int xs[kCoachColumns + 1] = {0, 40, 80, 120, 160, 200};
    playerColumns(c, stones, count, kCoachSeat, xs, kCoachColumns, activeCol, bottom, kCoachRows, kNarrowFont);
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
    if (focus == Focus::Players) focusBox(c, 0, 47, W - 1, bottom + 1);
    else focusBox(c, 0, bottom, W - 1, H - 1);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, 49, bottom, tip);
    if (tip == Tip::Move) moveTip(c, bottom, H);  // over the rocks
    if (tip == Tip::SwitchOn) {
        const char* const lines[6] = {"Double click rocker to", "switch page", "", "Hold rocker down: rocks",
                                      "Hold rocker up: move last time", "Hold rocker middle: undo"};
        tooltip(c, lines, 6, 49, bottom);
    }
}

void drawCoachRocks(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots, Waiting waiting,
                    Tip tip, Focus focus) {
    c.clear();
    const Font& label = kFontSpleen6x12;
    textInBox(c, label, "Back2Hog", 0, WIDE_MID, 0, LABEL_H, kCentre, 4);
    textInBox(c, label, "Hog2Hog", WIDE_MID, W - 18, 0, LABEL_H, kCentre, 4);
    hline(c, 0, W - 1, LABEL_H);
    pageMarks(c, 1);
    newestRow(c, stones, count, waiting, WIDE_MID, kFontDseg16, 13, 33, 20, kStopwatchSmall, 25);
    vline(c, WIDE_MID, 0, 33);
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
    if (focus == Focus::Players) focusBox(c, 0, 12, W - 1, 34);
    else focusBox(c, 0, 33, W - 1, 186);
    if (tip == Tip::Undo || tip == Tip::PowerOff) undoTip(c, 47, 185, tip);  // over the rocks
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
