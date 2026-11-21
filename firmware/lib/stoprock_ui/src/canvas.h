// 200 x 200 1-bit framebuffer with the few drawing primitives the screens need.
#pragma once
#include <stdint.h>
#include <string.h>

#include "assets.h"

namespace stoprock {

struct Rect {
    int16_t x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
};

struct InkBox {
    int l = 0, t = 0, r = 0, b = 0;  // r and b exclusive; l == r when empty
};

class Canvas {
public:
    static constexpr int W = 200, H = 200, STRIDE = W / 8;

    void clear() { memset(buf_, 0, sizeof buf_); }
    bool get(int x, int y) const;
    void set(int x, int y, bool black = true);

    void fillRect(int x0, int y0, int x1, int y1, bool black = true);  // inclusive corners
    void line(float x0, float y0, float x1, float y1, int width = 1, bool black = true);
    // Ellipse inscribed in [cx - r, cx + r] (Pillow's convention), outline of the given width.
    void ring(float cx, float cy, float r, int width);
    void blit(const Bitmap& bm, int x, int y);
    void sprite(const Sprite& s, int cx, int cy) { blit(s.bm, cx - s.cx, cy - s.cy); }

    // Text: origin is where Pillow would place it. Returns the pen advance.
    int text(const Font& f, int x, int y, const char* s, bool black = true);
    InkBox ink(const Font& f, const char* s) const;

    // Bounding box of pixels that differ from other, widened to 8-px columns.
    Rect diff(const Canvas& other) const;

    const uint8_t* data() const { return buf_; }
    uint8_t* data() { return buf_; }

private:
    uint8_t buf_[STRIDE * H];  // 1 = black
};

const Glyph* findGlyph(const Font& f, uint16_t code);

}  // namespace stoprock
