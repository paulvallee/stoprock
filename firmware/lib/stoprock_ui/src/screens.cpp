// Port of tools/mockups/screen.py. Keep the two in step: the mockups are the reference.
#include "screens.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace stoprock {
namespace {

constexpr int W = 200, H = 200;
constexpr int BH_X0 = 0, HH_X0 = 72, SH_X0 = 152;
constexpr int LABEL_H = 12, BIG_H = 34, ROW_H = 15, TIP_H = 26;
constexpr int ROCKS_BOTTOM = H - TIP_H;  // the 8 ends sit above this; the percentage row and tip strip below it

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
    void digit(Canvas& c, char ch, int x, int y) const {
        static const char* const kSegs[10] = {"abcdef", "bc", "abged", "abgcd", "fgbc",
                                              "afgcd", "afgedc", "abc", "abcdefg", "abcdfg"};
        if (ch < '0' || ch > '9') return;
        const char* segs = kSegs[ch - '0'];
        const int m = (h - t) / 2, k = (t == 1) ? 1 : t - 1, extra = (t == 1) ? 1 : 0;
        auto r = [&](int x0, int y0, int x1, int y1) { c.fillRect(x + x0, y + y0, x + x1 - 1, y + y1 - 1); };
        if (strchr(segs, 'a')) r(k, 0, w - k, t);
        if (strchr(segs, 'g')) r(k, m, w - k, m + t);
        if (strchr(segs, 'd')) r(k, h - t, w - k, h);
        if (strchr(segs, 'f')) r(0, k, t, m + t - k + extra);
        if (strchr(segs, 'b')) r(w - t, k, w, m + t - k + extra);
        if (strchr(segs, 'e')) r(0, m + k, t, h - k);
        if (strchr(segs, 'c')) r(w - t, m + k, w, h - k);
    }
    void draw(Canvas& c, const char* s, int x, int y) const {
        for (; *s; ++s) {
            if (*s == '.') c.fillRect(x, y + h - dot, x + dot - 1, y + h - 1);
            else if (*s == '1' && prop) digit(c, *s, x - (w - t), y);
            else digit(c, *s, x, y);
            x += charWidth(*s) + gap;
        }
    }
};

constexpr SegFont kRowFont{12, 7, 2, 2, 2, false};  // older results
constexpr SegFont kEndFont{9, 5, 1, 1, 1, false};   // end numbers: equal-width cells, a "1" sits on the right
constexpr SegFont kGridFont{12, 6, 2, 1, 2, true};   // competition history, three to a row

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
              Align a, double margin) {
    const int w = f.width(s);
    double x = a == kRight ? x1 - margin - w : a == kLeft ? x0 + margin : floor((x0 + x1 - w) / 2);
    double y = floor((y0 + y1 - f.h) / 2);
    f.draw(c, s, static_cast<int>(x), static_cast<int>(y));
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

void hline(Canvas& c, int x0, int x1, int y, int width = 1) { c.fillRect(x0, y, x1, y + width - 1); }
void vline(Canvas& c, int x, int y0, int y1, int width = 1) { c.fillRect(x, y0, x + width - 1, y1); }

}  // namespace

void drawMain(Canvas& c, const MainView& v) {
    c.clear();
    const bool tip = v.tip1 != nullptr;
    const int bottom = tip ? H - TIP_H : H;
    const Font& label = kFontSpleen6x12;

    textInBox(c, label, "Back2Hog", BH_X0, HH_X0, 0, LABEL_H, kCentre, 4);
    textInBox(c, label, "Hog2Hog", HH_X0, SH_X0, 0, LABEL_H, kCentre, 4);
    textInBox(c, label, "SHOTS", SH_X0, W, 0, LABEL_H, kCentre, 4);
    hline(c, 0, W - 1, LABEL_H);
    vline(c, HH_X0, 0, bottom - 1);
    vline(c, SH_X0, 0, H - 1, 2);

    // newest result, large
    const int y0 = LABEL_H + 1;
    if (v.stoneCount > 0) {
        if (v.stones[0].bh >= 0) dsegNumber(c, v.stones[0].bh, BH_X0, HH_X0, y0, y0 + BIG_H);
        if (v.stones[0].hh >= 0) dsegNumber(c, v.stones[0].hh, HH_X0, SH_X0, y0, y0 + BIG_H);
    }
    const int yh = y0 + BIG_H;
    hline(c, 0, SH_X0 - 1, yh, 2);

    // older results
    char buf[12];
    for (int i = 1; i < v.stoneCount; ++i) {
        const int ry = yh + 2 + (i - 1) * ROW_H;
        if (ry + ROW_H > bottom) break;
        if (v.stones[i].bh >= 0) {
            formatTime(v.stones[i].bh, buf, sizeof buf);
            segInBox(c, kRowFont, buf, BH_X0, HH_X0, ry, ry + ROW_H, kRight, 6);
        }
        if (v.stones[i].hh >= 0) {
            formatTime(v.stones[i].hh, buf, sizeof buf);
            segInBox(c, kRowFont, buf, HH_X0, SH_X0, ry, ry + ROW_H, kRight, 6);
        }
    }

    // rocks: one row per end, ring = cursor on the next slot
    const ShotLog empty;
    const ShotLog& shots = v.shots ? *v.shots : empty;
    const int cursor = shots.cursor();
    const double top = LABEL_H + 1, pitch = (ROCKS_BOTTOM - top) / 8;
    for (int end = 0; end < 8; ++end) {
        const double cy = top + pitch * end + pitch / 2;
        char n[3];
        snprintf(n, sizeof n, "%d", end + 1);
        // centred in the gap between the divider and the rocks, on the rocks' centre line
        kEndFont.draw(c, n, SH_X0 + 4, static_cast<int>(cy) - 4);
        for (int j = 0; j < 2; ++j) {
            const int k = end * 2 + j;
            const int cx = SH_X0 + (j == 0 ? 18 : 37);
            const int icy = static_cast<int>(cy);  // Pillow truncates fractional coordinates
            c.sprite(kRockOutline, cx, icy);
            if (k < shots.count()) c.sprite(shots.made(k) ? kCheck : kCross, cx, icy);
            if (k == cursor) c.sprite(kCursorRing, cx, icy);
        }
    }

    // percentage: DSEG7 digits packed by their ink, % sign drawn to match
    // one line straight across: over the tip strip and the percentage row alike
    const double y9 = ROCKS_BOTTOM;
    hline(c, 0, W - 1, ROCKS_BOTTOM, 2);
    if (shots.count() > 0) {
        char n[4];
        snprintf(n, sizeof n, "%d", shots.percent());
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
        double x = (SH_X0 + 2 + W - total) / 2;
        const double ytop = (y9 + 1 + H - dh) / 2;
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

    // tip strip, white on black for warnings
    if (tip) {
        const int ty = H - TIP_H;
        if (v.warning) c.fillRect(0, ty, SH_X0 - 1, H - 1);
        c.text(label, 1, ty + 2, v.tip1, !v.warning);
        if (v.tip2) c.text(label, 1, ty + 14, v.tip2, !v.warning);
    }
}

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
    eqLine("nothing     = competition");
    eqLine("rocker down = league");
    eqLine("rocker up   = coach");
    y += 5;
    tag("TIMING", "use top or side button");
    y += 3;
    tag("SHOTS", "up/down = made/missed");
    eqLine("hold = undo last shot");
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

// Competition history: a column per teammate in each half, headed with their position; the
// active one's header is highlighted. H2H times are a digit wider, so that half is wider (x 92).
constexpr int COMP_MID = 92;
constexpr int GRID_LABEL_Y0 = 49, GRID_LABEL_Y1 = 60;
constexpr int GRID_TOP = 61, GRID_PITCH = 14;
constexpr int GRID_TYPICAL[2] = {23, 26};  // width of "3.47" and "13.92": each column centres that width

void gridColumns(int half, int mid, int* edges) {
    const int x0 = half == 0 ? 0 : mid, x1 = half == 0 ? mid : W;
    for (int k = 0; k < 4; ++k) edges[k] = x0 + (x1 - x0) * k / 3;
}

void timesGrid(Canvas& c, const Stone* stones, int count, int bottom, int mid, int me, int active) {
    int cols[2][4];
    gridColumns(0, mid, cols[0]);
    gridColumns(1, mid, cols[1]);
    for (int half = 0; half < 2; ++half) {
        for (int k = 0; k < 3; ++k) {
            const int x0 = cols[half][k], x1 = cols[half][k + 1];
            const bool on = k == active;
            if (on) c.fillRect(x0, GRID_LABEL_Y0, x1 - 1, GRID_LABEL_Y1);
            const char* name = kPositions[teammate(me, k)];  // on one baseline, centred by the 6 px cells
            c.text(kFontSpleen6x12, (x0 + x1 - 6 * static_cast<int>(strlen(name))) / 2, GRID_LABEL_Y0, name, !on);
        }
    }
    (void)bottom;
    char buf[12];
    int rows[3] = {0, 0, 0};
    for (int i = 0; i < count; ++i) {
        const int p = stones[i].player;
        if (p > 3 || p == me) continue;
        const int k = p < me ? p : p - 1;
        if (rows[k] >= kGridRows) continue;
        const int ry = GRID_TOP + rows[k]++ * GRID_PITCH;
        const int32_t vals[2] = {stones[i].bh, stones[i].hh};
        for (int half = 0; half < 2; ++half) {
            if (vals[half] < 0) continue;
            const int x0 = cols[half][k], x1 = cols[half][k + 1];
            formatTime(vals[half], buf, sizeof buf);
            // centred; an odd pixel goes on the right
            segInBox(c, kGridFont, buf, x0, x1, ry, ry + GRID_PITCH, kRight, (x1 - x0 - GRID_TYPICAL[half] + 1) / 2);
        }
    }
}

// Back2Hog | Hog2Hog across the full width: header, newest large, older below
// (one to a row, or with grid three to a row).
void timesWide(Canvas& c, const Stone* stones, int count, int bottom, int labelRight, bool grid = false,
               int mid = WIDE_MID, int me = 0, int active = 0) {
    const Font& label = kFontSpleen6x12;
    textInBox(c, label, "Back2Hog", 0, mid, 0, LABEL_H, kCentre, 4);
    textInBox(c, label, "Hog2Hog", mid, labelRight, 0, LABEL_H, kCentre, 4);
    hline(c, 0, W - 1, LABEL_H);
    if (count > 0) {
        if (stones[0].bh >= 0) dsegNumber(c, stones[0].bh, 0, mid, 13, 47);
        if (stones[0].hh >= 0) dsegNumber(c, stones[0].hh, mid, W, 13, 47);
    }
    hline(c, 0, W - 1, 47, 2);
    vline(c, mid, 0, bottom - 1);
    if (grid) { timesGrid(c, stones, count, bottom, mid, me, active); return; }
    char buf[12];
    for (int i = 1; i < count; ++i) {
        const int ry = 50 + (i - 1) * 14;
        if (ry + 14 > bottom) break;
        if (stones[i].bh >= 0) {
            formatTime(stones[i].bh, buf, sizeof buf);
            segInBox(c, kRowFont, buf, 0, WIDE_MID, ry, ry + 14, kRight, 14);
        }
        if (stones[i].hh >= 0) {
            formatTime(stones[i].hh, buf, sizeof buf);
            segInBox(c, kRowFont, buf, WIDE_MID, W, ry, ry + 14, kRight, 14);
        }
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

void drawCompetition(Canvas& c, const Stone* stones, uint8_t count, uint8_t me, uint8_t activeCol,
                     bool lowBattery) {
    c.clear();
    const Font& label = kFontSpleen6x12;
    const int strip = H - TIP_H;
    timesWide(c, stones, count, strip, W, true, COMP_MID, me, activeCol);
    c.fillRect(0, strip, W - 1, H - 1);
    textInBox(c, label, "COMPETITION", 0, W, strip + 1, strip + 13, kCentre, 4, false);
    textInBox(c, label, lowBattery ? "Battery low" : "times only, radio off", 0, W, strip + 13, strip + 25,
              kCentre, 4, false);
}

void drawCoachTimer(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots) {
    c.clear();
    const Font& label = kFontSpleen6x12;
    pageMarks(c, 0);
    const int bottom = 150;
    timesWide(c, stones, count, bottom, W - 18);
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
}

void drawCoachRocks(Canvas& c, const Stone* stones, uint8_t count, const ShotLog& shots) {
    c.clear();
    const Font& label = kFontSpleen6x12;
    textInBox(c, label, "Back2Hog", 0, WIDE_MID, 0, LABEL_H, kCentre, 4);
    textInBox(c, label, "Hog2Hog", WIDE_MID, W - 18, 0, LABEL_H, kCentre, 4);
    hline(c, 0, W - 1, LABEL_H);
    pageMarks(c, 1);
    if (count > 0) {
        if (stones[0].bh >= 0) dsegNumber(c, stones[0].bh, 0, WIDE_MID, 13, 33, 20, kFontDseg16);
        if (stones[0].hh >= 0) dsegNumber(c, stones[0].hh, WIDE_MID, W, 13, 33, 20, kFontDseg16);
    }
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
    auto group = [](const Stone& s) { return s.player < 4 ? s.player : 4; };  // 4: no player
    int per[5] = {};
    for (int i = 0; i < capacity; ++i) ++per[group(list[i])];
    if (group(dropped) == 4 || per[group(dropped)] >= kGridRows) return;  // nothing to keep it for
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
