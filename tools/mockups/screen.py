"""Pixel-exact mockups of the stoprock screens (200 x 200, 1-bit), as agreed in docs/SPEC.md."""
from pathlib import Path
import tempfile

from PIL import BdfFontFile, Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
FONTS = ROOT / "assets" / "fonts"
DSEG = FONTS / "DSEG7Classic-Bold.ttf"

W = H = 200
BH_X0, HH_X0, SH_X0 = 0, 72, 152  # column edges on 8-px boundaries
LABEL_H = 12
BIG_H = 34
ROW_H = 15
TIP_H = 26
END_ROWS = 9  # 8 ends + percentage row
SLOTS = 16

_bdf_cache = {}


def bdf(name):
    """Load a BDF bitmap font (converted to Pillow's format in a temp dir)."""
    if name not in _bdf_cache:
        out = Path(tempfile.gettempdir()) / "stoprock-fonts"
        out.mkdir(exist_ok=True)
        with open(FONTS / f"{name}.bdf", "rb") as fp:
            BdfFontFile.BdfFontFile(fp).save(str(out / name))
        _bdf_cache[name] = ImageFont.load(str(out / f"{name}.pil"))
    return _bdf_cache[name]


# --------------------------------------------------------------------------- text helpers
_ink = {}


def ink(s, font):
    """Bounding box of the black pixels when s is drawn at (0, 0)."""
    key = (s, id(font))
    if key not in _ink:
        pad = 40
        im = Image.new("1", (len(s) * 80 + 2 * pad, 160), 1)
        dd = ImageDraw.Draw(im)
        dd.fontmode = "1"
        dd.text((pad, pad), s, font=font, fill=0)
        l, t, r, b = Image.eval(im.convert("L"), lambda v: 255 - v).getbbox()
        _ink[key] = (l - pad, t - pad, r - pad, b - pad)
    return _ink[key]


def text_in_box(d, s, font, x0, x1, y0, y1, align="right", margin=4):
    if isinstance(font, SegFont):
        return font.draw_in_box(d, s, x0, x1, y0, y1, align, margin)
    l, t, r, b = ink(s, font)
    w = r - l
    x = {"right": x1 - margin - w - l, "left": x0 + margin - l}.get(align, (x0 + x1 - w) / 2 - l)
    y = (y0 + y1 - (b - t)) / 2 - t
    d.text((round(x), round(y)), s, font=font, fill=0)


def dseg_number(d, val, font, x0, x1, y0, y1, margin=3):
    """Right-aligned DSEG7 number with a small square decimal point."""
    ip, fp = f"{val:.2f}".split(".")
    li, ti, ri, bi = ink(ip, font)
    lf, tf, rf, bf = ink(fp, font)
    t, b = min(ti, tf), max(bi, bf)
    dot, gap = max(3, round((b - t) * 0.13)), 2
    total = (ri - li) + gap + dot + gap + (rf - lf)
    x = x1 - margin - total
    y = (y0 + y1 - (b - t)) / 2 - t
    d.text((round(x - li), round(y)), ip, font=font, fill=0)
    xd = x + (ri - li) + gap
    yb = y + b
    d.rectangle([round(xd), round(yb - dot), round(xd + dot - 1), round(yb - 1)], fill=0)
    d.text((round(xd + dot + gap - lf), round(y)), fp, font=font, fill=0)


# --------------------------------------------------------------------------- drawn segments
SEGMENTS = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc",
            "5": "afgcd", "6": "afgedc", "7": "abc", "8": "abcdefg", "9": "abcdfg", "-": "g"}


class SegFont:
    """Seven-segment digits drawn from rectangles, with the corner pixels left empty."""

    def __init__(self, h, w, t, gap, dot=None, prop=False):
        self.h, self.w, self.t, self.gap, self.prop = h, w, t, gap, prop
        self.dot = dot or t

    def _width(self, ch):
        if ch == ".":
            return self.dot
        if ch == "1" and self.prop:
            return self.t
        return self.w

    def width(self, s):
        return sum(self._width(c) for c in s) + self.gap * (len(s) - 1)

    def _digit(self, d, ch, x, y):
        w, h, t = self.w, self.h, self.t
        segs = SEGMENTS.get(ch, "")
        m = (h - t) // 2
        c = 1 if t == 1 else t - 1
        extra = 1 if t == 1 else 0

        def r(x0, y0, x1, y1):
            d.rectangle([x + x0, y + y0, x + x1 - 1, y + y1 - 1], fill=0)

        if "a" in segs: r(c, 0, w - c, t)
        if "g" in segs: r(c, m, w - c, m + t)
        if "d" in segs: r(c, h - t, w - c, h)
        if "f" in segs: r(0, c, t, m + t - c + extra)
        if "b" in segs: r(w - t, c, w, m + t - c + extra)
        if "e" in segs: r(0, m + c, t, h - c)
        if "c" in segs: r(w - t, m + c, w, h - c)

    def draw(self, d, s, x, y):
        for ch in s:
            if ch == ".":
                d.rectangle([x, y + self.h - self.dot, x + self.dot - 1, y + self.h - 1], fill=0)
            elif ch == "1" and self.prop:
                self._digit(d, ch, x - (self.w - self.t), y)
            else:
                self._digit(d, ch, x, y)
            x += self._width(ch) + self.gap

    def draw_in_box(self, d, s, x0, x1, y0, y1, align="right", margin=4):
        w = self.width(s)
        x = {"right": x1 - margin - w, "left": x0 + margin}.get(align, (x0 + x1 - w) // 2)
        self.draw(d, s, x, (y0 + y1 - self.h) // 2)


def fonts():
    return dict(
        label=bdf("spleen-6x12"),
        big=ImageFont.truetype(str(DSEG), 25),
        row=SegFont(h=12, w=7, t=2, gap=2, dot=2),
        tiny=SegFont(h=9, w=5, t=1, gap=1, prop=True),
        pct=ImageFont.truetype(str(DSEG), 16),
    )


# --------------------------------------------------------------------------- main screen
def rock_xy(k):
    top = LABEL_H + 1
    pitch = (H - top) / END_ROWS
    end, j = divmod(k, 2)
    return SH_X0 + (18 if j == 0 else 37), top + pitch * end + pitch / 2


def main_screen(stones, shots, tip=None, warn=False):
    """stones: newest first, each (bh, hh) with None for a split not timed.
    shots: True made / False missed, in throw order. The ring marks the next slot."""
    fs = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    bottom = H - TIP_H if tip else H

    for label, x0, x1 in (("Back2Hog", BH_X0, HH_X0), ("Hog2Hog", HH_X0, SH_X0), ("SHOTS", SH_X0, W)):
        text_in_box(d, label, fs["label"], x0, x1, 0, LABEL_H, align="center")
    d.line([(0, LABEL_H), (W - 1, LABEL_H)], fill=0)
    d.line([(HH_X0, 0), (HH_X0, bottom - 1)], fill=0)
    d.line([(SH_X0, 0), (SH_X0, H - 1)], fill=0, width=2)

    # newest result, large
    y0 = LABEL_H + 1
    if stones:
        for val, x0, x1 in zip(stones[0], (BH_X0, HH_X0), (HH_X0, SH_X0)):
            if val is not None:
                dseg_number(d, val, fs["big"], x0, x1, y0, y0 + BIG_H)
    yh = y0 + BIG_H
    d.line([(0, yh), (SH_X0 - 1, yh)], fill=0, width=2)

    # older results
    for i, stone in enumerate(stones[1:]):
        ry = yh + 2 + i * ROW_H
        if ry + ROW_H > bottom:
            break
        for val, x0, x1 in zip(stone, (BH_X0, HH_X0), (HH_X0, SH_X0)):
            if val is not None:
                text_in_box(d, f"{val:.2f}", fs["row"], x0, x1, ry, ry + ROW_H, margin=6)

    # rocks, one row per end; ring = cursor on the next slot
    cursor = len(shots) if len(shots) < SLOTS else -1
    top = LABEL_H + 1
    pitch = (H - top) / END_ROWS
    r = 7
    for end in range(8):
        cy = top + pitch * end + pitch / 2
        text_in_box(d, str(end + 1), fs["tiny"], SH_X0 + 1, SH_X0 + 9, cy - 6, cy + 6, align="left", margin=2)
        for j in range(2):
            k = end * 2 + j
            cx, _ = rock_xy(k)
            d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=0, width=1)
            if k < len(shots):
                if shots[k]:
                    d.line([(cx - 4, cy), (cx - 1, cy + 3), (cx + 4, cy - 4)], fill=0, width=2)
                else:
                    d.line([(cx - 3, cy - 3), (cx + 3, cy + 3)], fill=0, width=2)
                    d.line([(cx - 3, cy + 3), (cx + 3, cy - 3)], fill=0, width=2)
            if k == cursor:
                d.ellipse([cx - 10, cy - 10, cx + 10, cy + 10], outline=0, width=2)

    # percentage: DSEG7 digits packed by their ink, % sign drawn to match
    y9 = top + pitch * 8
    d.line([(SH_X0 + 2, y9), (W - 1, y9)], fill=0, width=2)
    if shots:
        n = str(round(100 * sum(shots) / len(shots)))
        f = fs["pct"]
        boxes = [ink(c, f) for c in n]
        t, b = min(bx[1] for bx in boxes), max(bx[3] for bx in boxes)
        dh = b - t
        pw, st, q = round(dh * 0.6), 2, 3
        gap = 2 if len(n) > 2 else 3
        total = sum(bx[2] - bx[0] for bx in boxes) + gap * len(n) + pw
        x = (SH_X0 + 2 + W - total) / 2
        ytop = (y9 + 1 + H - dh) / 2
        for c, bx in zip(n, boxes):
            d.text((round(x - bx[0]), round(ytop - t)), c, font=f, fill=0)
            x += (bx[2] - bx[0]) + gap
        x, y = round(x), round(ytop)
        d.rectangle([x, y, x + q - 1, y + q - 1], fill=0)
        d.rectangle([x + pw - q, y + dh - q, x + pw - 1, y + dh - 1], fill=0)
        d.line([(x + pw - 1, y), (x, y + dh - 1)], fill=0, width=st)

    # tip strip (white on black for warnings)
    if tip:
        ty = H - TIP_H
        if warn:
            d.rectangle([0, ty, SH_X0 - 1, H - 1], fill=0)
        else:
            d.line([(0, ty), (SH_X0 - 1, ty)], fill=0, width=2)
        for i, line in enumerate(tip):
            d.text((1, ty + 2 + i * 12), line, font=fs["label"], fill=1 if warn else 0)
    return img


# --------------------------------------------------------------------------- help page
def help_page(device_logo, last_game, battery=None):
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    f = bdf("spleen-6x12")
    img.paste(device_logo, ((W - device_logo.width) // 2, 4))
    y = 4 + device_logo.height + 6

    def line(txt, x=4):
        nonlocal y
        d.text((x, y), txt, font=f, fill=0)
        y += 12

    def tag(label, rest):
        nonlocal y
        w = len(label) * 6
        d.rectangle([3, y, 4 + w + 2, y + 12], fill=0)
        d.text((5, y + 1), label, font=f, fill=1)
        d.text((5 + w + 8, y + 1), rest, font=f, fill=0)
        y += 14

    t = "HOLD SIDE BUTTON TO START"
    d.text(((W - len(t) * 6) // 2, y), t, font=f, fill=0)
    y += 18
    tag("TIMING", "top or side button")
    line(" click back line, hog, far hog")
    line(" B2H back or tee to hog line")
    line(" H2H hog line to hog line")
    y += 5
    tag("SHOTS", "rocker")
    line(" up = made     down = missed")
    line(" hold 0.5 s = undo last shot")
    y += 4
    d.line([(4, y), (195, y)], fill=0)
    y += 4
    if battery is not None:  # battery at switch-off, bottom right: icon and percentage
        pct = f"{battery}%"
        bx = W - 4 - (17 + 6 * len(pct))
        d.rectangle([bx, y + 3, bx + 12, y + 9], outline=0)
        d.rectangle([bx + 13, y + 5, bx + 14, y + 7], fill=0)
        fill = round(9 * max(0, min(100, battery)) / 100)
        if fill > 0:
            d.rectangle([bx + 2, y + 5, bx + 2 + fill - 1, y + 7], fill=0)
        d.text((bx + 17, y), pct, font=f, fill=0)
    line("Off after 5 min idle")
    line(last_game)
    assert y <= H, f"help page overflows: {y}"
    return img


def to_rgb(img, z):
    """Enlarge a 1-bit screen and tint it like e-ink."""
    big = img.resize((img.width * z, img.height * z), Image.NEAREST)
    out = Image.new("RGB", big.size, (222, 222, 214))
    out.paste(Image.new("RGB", big.size, (25, 25, 25)), (0, 0),
              big.convert("L").point(lambda v: 255 if v < 128 else 0))
    return out
