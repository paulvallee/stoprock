#include "canvas.h"

#include <math.h>

namespace stoprock {

bool Canvas::get(int x, int y) const {
    if (x < 0 || y < 0 || x >= W || y >= H) return false;
    return buf_[y * STRIDE + x / 8] & (0x80 >> (x % 8));
}

void Canvas::set(int x, int y, bool black) {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    uint8_t& b = buf_[y * STRIDE + x / 8];
    const uint8_t m = 0x80 >> (x % 8);
    b = black ? (b | m) : (b & ~m);
}

void Canvas::fillRect(int x0, int y0, int x1, int y1, bool black) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) set(x, y, black);
}

void Canvas::line(float x0, float y0, float x1, float y1, int width, bool black) {
    if (width <= 1) {  // Bresenham
        int ax = lroundf(x0), ay = lroundf(y0), bx = lroundf(x1), by = lroundf(y1);
        int dx = abs(bx - ax), sx = ax < bx ? 1 : -1;
        int dy = -abs(by - ay), sy = ay < by ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            set(ax, ay, black);
            if (ax == bx && ay == by) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; ax += sx; }
            if (e2 <= dx) { err += dx; ay += sy; }
        }
        return;
    }
    // Thick line: pixel centres within width/2 of the segment, between its end caps.
    const float vx = x1 - x0, vy = y1 - y0;
    const float len2 = vx * vx + vy * vy;
    const float half = width / 2.0f;
    const int minx = floorf(fminf(x0, x1) - half), maxx = ceilf(fmaxf(x0, x1) + half);
    const int miny = floorf(fminf(y0, y1) - half), maxy = ceilf(fmaxf(y0, y1) + half);
    for (int y = miny; y <= maxy; ++y) {
        for (int x = minx; x <= maxx; ++x) {
            float t = len2 > 0 ? ((x - x0) * vx + (y - y0) * vy) / len2 : 0;
            if (t < 0 || t > 1) continue;
            float px = x0 + t * vx - x, py = y0 + t * vy - y;
            if (px * px + py * py <= half * half) set(x, y, black);
        }
    }
}

void Canvas::ring(float cx, float cy, float r, int width) {
    const float ro = r + 0.5f, ri = r + 0.5f - width;
    const int x0 = floorf(cx - ro), x1 = ceilf(cx + ro), y0 = floorf(cy - ro), y1 = ceilf(cy + ro);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
            if (d2 <= ro * ro && d2 > ri * ri) set(x, y);
        }
    }
}

void Canvas::blit(const Bitmap& bm, int x, int y) {
    const int stride = (bm.w + 7) / 8;
    for (int j = 0; j < bm.h; ++j)
        for (int i = 0; i < bm.w; ++i)
            if (bm.bits[j * stride + i / 8] & (0x80 >> (i % 8))) set(x + i, y + j);
}

const Glyph* findGlyph(const Font& f, uint16_t code) {
    for (uint16_t i = 0; i < f.count; ++i)
        if (f.glyphs[i].code == code) return &f.glyphs[i];
    return nullptr;
}

// Minimal UTF-8 decoding: ASCII plus two-byte sequences (enough for the degree sign).
static uint16_t nextCode(const char*& s) {
    uint8_t c = static_cast<uint8_t>(*s++);
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && *s) return static_cast<uint16_t>(((c & 0x1F) << 6) | (static_cast<uint8_t>(*s++) & 0x3F));
    return '?';
}

int Canvas::text(const Font& f, int x, int y, const char* s, bool black) {
    int pen = x;
    while (*s) {
        const Glyph* g = findGlyph(f, nextCode(s));
        if (!g) continue;
        const int stride = (g->w + 7) / 8;
        const uint8_t* bits = f.bits + g->off;
        for (int j = 0; j < g->h; ++j)
            for (int i = 0; i < g->w; ++i)
                if (bits[j * stride + i / 8] & (0x80 >> (i % 8))) set(pen + g->dx + i, y + g->dy + j, black);
        pen += g->adv;
    }
    return pen - x;
}

InkBox Canvas::ink(const Font& f, const char* s) const {
    InkBox box;
    bool any = false;
    int pen = 0;
    while (*s) {
        const Glyph* g = findGlyph(f, nextCode(s));
        if (!g) continue;
        if (g->w && g->h) {
            const int l = pen + g->dx, t = g->dy, r = l + g->w, b = t + g->h;
            if (!any) { box = {l, t, r, b}; any = true; }
            else {
                if (l < box.l) box.l = l;
                if (t < box.t) box.t = t;
                if (r > box.r) box.r = r;
                if (b > box.b) box.b = b;
            }
        }
        pen += g->adv;
    }
    return box;
}

Rect Canvas::diff(const Canvas& o) const {
    int cx0 = STRIDE, cx1 = -1, y0 = H, y1 = -1;
    for (int y = 0; y < H; ++y) {
        for (int c = 0; c < STRIDE; ++c) {
            if (buf_[y * STRIDE + c] != o.buf_[y * STRIDE + c]) {
                if (c < cx0) cx0 = c;
                if (c > cx1) cx1 = c;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        }
    }
    Rect r;
    if (cx1 < 0) return r;
    r.x = cx0 * 8;
    r.w = (cx1 - cx0 + 1) * 8;
    r.y = y0;
    r.h = y1 - y0 + 1;
    return r;
}

}  // namespace stoprock
