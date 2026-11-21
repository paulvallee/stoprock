// Port of tools/mockups/screen.py. Keep the two in step: the mockups are the reference.
#include "screens.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace stoprock {
namespace {

constexpr int W = 200, H = 200;
constexpr int BH_X0 = 0, HH_X0 = 72, SH_X0 = 152;
constexpr int LABEL_H = 12, BIG_H = 34, ROW_H = 15, TIP_H = 26, END_ROWS = 9;

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
constexpr SegFont kEndFont{9, 5, 1, 1, 1, true};    // end numbers

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
void dsegNumber(Canvas& c, int32_t hundredths, double x0, double x1, double y0, double y1, double margin = 3) {
    const Font& f = kFontDseg25;
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
    const double top = LABEL_H + 1, pitch = (H - top) / END_ROWS;
    for (int end = 0; end < 8; ++end) {
        const double cy = top + pitch * end + pitch / 2;
        char n[3];
        snprintf(n, sizeof n, "%d", end + 1);
        segInBox(c, kEndFont, n, SH_X0 + 1, SH_X0 + 9, cy - 6, cy + 6, kLeft, 2);
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
    const double y9 = top + pitch * 8;
    hline(c, SH_X0 + 2, W - 1, static_cast<int>(y9), 2);
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
        else hline(c, 0, SH_X0 - 1, ty, 2);
        c.text(label, 1, ty + 2, v.tip1, !v.warning);
        if (v.tip2) c.text(label, 1, ty + 14, v.tip2, !v.warning);
    }
}

void drawHelp(Canvas& c, const char* lastGame) {
    c.clear();
    const Font& f = kFontSpleen6x12;
    c.blit(kLogo, (W - kLogo.w) / 2, 4);
    int y = 4 + kLogo.h + 6;

    auto line = [&](const char* s) { c.text(f, 4, y, s); y += 12; };
    auto tag = [&](const char* labelText, const char* rest) {
        const int w = static_cast<int>(strlen(labelText)) * 6;
        c.fillRect(3, y, 4 + w + 2, y + 12);
        c.text(f, 5, y + 1, labelText, false);
        c.text(f, 5 + w + 8, y + 1, rest);
        y += 14;
    };

    const char* head = "HOLD SIDE BUTTON TO START";
    c.text(f, (W - static_cast<int>(strlen(head)) * 6) / 2, y, head);
    y += 18;
    tag("TIMING", "top or side button");
    line(" click back line, hog, far hog");
    line(" BH  back line to hog line");
    line(" HH  hog line to hog line");
    y += 5;
    tag("SHOTS", "rocker");
    line(" up = made     down = missed");
    line(" hold 0.5 s = undo last shot");
    y += 4;
    c.line(4, y, 195, y);
    y += 4;
    line("Switches off after 5 min idle");
    line(lastGame);
}

}  // namespace stoprock
