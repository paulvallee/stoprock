"""Revised tutorial cards (a proposal, alongside the current ones in tools/tutorials.py).

    python3 tools/tutorials_claude/build.py

Writes one 200 x 200 1-bit PNG per card to tools/tutorials_claude/cards/ and enlarged contact sheets
to tools/tutorials_claude/sheets/. Shapes and arrows are drawn at 4x and reduced to 1 bit, so curves
come out smooth; text is drawn at 1x in 1 bit, so it stays crisp. Nothing here touches the firmware.
"""
import json
import math
import sys
from functools import cache
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / "tools" / "mockups"))
import screen  # noqa: E402  (DSEG digits and the e-ink tint, as on the device)

NUNITO = ROOT / "assets" / "fonts" / "Nunito[wght].ttf"
W = H = 200
S = 4                        # shapes are drawn at S x, then reduced
DEVICE = json.loads((ROOT / "hardware" / "coreink" / "coreink.json").read_text())

# Layout
TITLE_Y = 15                 # the title's top
RULE_Y = 42
BODY = (44, 134)             # the illustration's band
COPY_Y = (138, 156)          # two lines of copy
FOOT_Y = 177                 # the footer bar's top


@cache
def nunito(size, weight="Bold"):
    f = ImageFont.truetype(str(NUNITO), size)
    f.set_variation_by_name(weight)
    return f


# --------------------------------------------------------------------------- drawing
class Card:
    """One card: shapes on a 4x greyscale layer, text on top at 1x once it's reduced to 1 bit."""

    def __init__(self):
        self.hi = Image.new("L", (W * S, H * S), 255)
        self.d = ImageDraw.Draw(self.hi)
        self.texts = []      # (text, font, x, y, fill, bold) drawn after the reduction

    # shapes, in 1x coordinates (floats are fine)
    def _p(self, *xy):
        return [v * S for v in xy]

    def rrect(self, x0, y0, x1, y1, r, fill=None, outline=0, width=1.5):
        self.d.rounded_rectangle(self._p(x0, y0, x1, y1), radius=r * S, fill=fill,
                                 outline=outline, width=round(width * S) if outline is not None else 0)

    def rect(self, x0, y0, x1, y1, fill=0):
        self.d.rectangle(self._p(x0, y0, x1, y1), fill=fill)

    def disc(self, cx, cy, r, fill=0):
        self.d.ellipse(self._p(cx - r, cy - r, cx + r, cy + r), fill=fill)

    def ring(self, cx, cy, r, width=1.5, fill=None):
        self.d.ellipse(self._p(cx - r, cy - r, cx + r, cy + r), outline=0, width=round(width * S), fill=fill)

    def poly(self, pts, fill=0):
        self.d.polygon([(x * S, y * S) for x, y in pts], fill=fill)

    def line(self, pts, width=1.5, fill=0):
        self.d.line([(x * S, y * S) for x, y in pts], fill=fill, width=round(width * S), joint="curve")
        for x, y in (pts[0], pts[-1]):  # round caps
            self.disc(x, y, width / 2, fill)

    def arrow(self, start, c1, c2, end, width=2.6, head=8.0, fill=0):
        """A smooth cubic curve from start to end (controls c1, c2), a solid head at end."""
        def at(t):
            u = 1 - t
            return (u ** 3 * start[0] + 3 * u * u * t * c1[0] + 3 * u * t * t * c2[0] + t ** 3 * end[0],
                    u ** 3 * start[1] + 3 * u * u * t * c1[1] + 3 * u * t * t * c2[1] + t ** 3 * end[1])
        pts = [at(i / 48) for i in range(49)]
        tx, ty = end[0] - pts[-4][0], end[1] - pts[-4][1]   # the direction it arrives in
        n = math.hypot(tx, ty)
        ux, uy = tx / n, ty / n
        base = (end[0] - head * ux, end[1] - head * uy)
        # stop the shaft inside the head so its round cap doesn't show past it
        shaft = [p for p in pts if (p[0] - end[0]) * ux + (p[1] - end[1]) * uy < -head * 0.6]
        self.line(shaft, width, fill)
        half = head * 0.62
        self.poly([end, (base[0] + half * uy, base[1] - half * ux), (base[0] - half * uy, base[1] + half * ux)], fill)

    # text (1x, 1 bit)
    def text(self, s, font, x, y, fill=0, anchor="la", bold=False):
        self.texts.append((s, font, x, y, fill, anchor, bold))

    def ctext(self, s, font, cx, y, fill=0, bold=False):
        self.text(s, font, cx, y, fill, "ma", bold)

    def finish(self):
        small = self.hi.resize((W, H), Image.LANCZOS)
        img = small.point(lambda v: 255 if v >= 128 else 0).convert("1")
        d = ImageDraw.Draw(img)
        d.fontmode = "1"
        for s, font, x, y, fill, anchor, bold in self.texts:
            for dx in ((0, 1) if bold else (0,)):
                d.text((round(x) + dx, round(y)), s, font=font, fill=1 if fill else 0, anchor=anchor)
        return img


def width_of(s, font):
    l, _, r, _ = font.getbbox(s)
    return r - l


# --------------------------------------------------------------------------- parts
def rocker(c, x, y, lit=(), hold=None, scale=1.0):
    """The rocker as a capsule (up, middle, down), lit parts black. hold: a "hold" tag beside a part."""
    w, h = 24 * scale, 66 * scale
    c.rrect(x, y, x + w, y + h, w / 2, fill=255, outline=0, width=1.6)
    seg = h / 3
    for i, part in enumerate(("up", "middle", "down")):
        top, cx, cy = y + i * seg, x + w / 2, y + i * seg + seg / 2
        on = part in lit
        if on:
            inset = 3 * scale
            r = (w / 2 - inset) if part != "middle" else 4 * scale
            c.rrect(x + inset, top + inset * (0.6 if part != "up" else 1), x + w - inset,
                    top + seg - inset * (0.6 if part != "down" else 1), r, fill=0, outline=None)
        ink = 255 if on else 0
        k = 5 * scale
        if part == "up":
            c.poly([(cx, cy - k * 0.8), (cx - k, cy + k * 0.6), (cx + k, cy + k * 0.6)], ink)
        elif part == "down":
            c.poly([(cx, cy + k * 0.8), (cx - k, cy - k * 0.6), (cx + k, cy - k * 0.6)], ink)
        else:
            c.disc(cx, cy, 3.2 * scale, ink)
    if hold:
        part, side = hold
        i = ("up", "middle", "down").index(part)
        cy = y + i * seg + seg / 2
        tag = nunito(11, "ExtraBold")
        tw = width_of("hold", tag) + 10
        if side in ("left", "right"):
            tx = x + w + 6 if side == "right" else x - 6 - tw
        else:  # below the capsule, centred
            tx, cy = x + w / 2 - tw / 2, y + h + 12
        c.rrect(tx, cy - 8, tx + tw, cy + 8, 8, fill=0, outline=None)
        c.ctext("hold", tag, tx + tw / 2, cy - 7, fill=1)
    return (x, y, x + w, y + h)


def badge(c, cx, cy, s, r=10):
    """A black disc with white text: "2x", "1s", "3s"."""
    c.disc(cx, cy, r)
    f = nunito(11 if len(s) < 3 else 10, "ExtraBold")
    c.ctext(s, f, cx, cy - 7, fill=1)


def pill(c, x0, y0, x1, y1, s, font, black=False, r=6):
    c.rrect(x0, y0, x1, y1, r, fill=0 if black else 255, outline=0, width=1.5)
    c.ctext(s, font, (x0 + x1) / 2, y0 + (y1 - y0 - font.size) / 2 - 1, fill=1 if black else 0)


def time_pill(c, x0, y0, x1, y1, value, h2h):
    """A time as the screen shows it: B2H black on white, H2H white on black."""
    f = nunito(13, "ExtraBold")
    pill(c, x0, y0, x1, y1, value, f, black=h2h, r=5)


def rock(c, cx, cy, state, r=10):
    """A rock as on the screen: filled made, a dot in the ring missed."""
    if state == "made":
        c.disc(cx, cy, r)
    else:
        c.ring(cx, cy, r, width=2)
        if state == "missed":
            c.disc(cx, cy, r * 0.32)


def stopwatch(c, cx, cy, r, fill=0):
    c.d.ellipse(c._p(cx - r, cy - r, cx + r, cy + r), outline=fill, width=round(1.8 * S))
    c.rect(cx - 1.2, cy - r - 3, cx + 1.2, cy - r, fill)
    c.rrect(cx - 3.5, cy - r - 5, cx + 3.5, cy - r - 2.5, 1, fill=fill, outline=None)
    c.line([(cx, cy), (cx + r * 0.45, cy - r * 0.55)], 1.6, fill)


def device(c, x0, y0, scale, lit=(), rocker_marks=True):
    """The CoreInk from the front (coreink.json), its buttons drawn where they are. lit: "top",
    "side": black; the rocker stays outlined (it's not a timing button)."""
    bw, bh = DEVICE["body"]["width"], DEVICE["body"]["height"]

    def X(mm):
        return x0 + mm * scale

    def Y(mm):  # coreink.json's y is up from the bottom
        return y0 + (bh - mm) * scale

    # buttons first, so the body's outline sits over their inner edge
    tb = DEVICE["top"]["button"]["x"]
    c.rrect(X(tb[0]), Y(bh) - 4, X(tb[1]), Y(bh) + 3, 1.5, fill=0 if "top" in lit else 255, outline=0, width=1.4)
    pb = DEVICE["right"]["power_button"]["y"]
    c.rrect(X(bw) - 3, Y(pb[1]), X(bw) + 4, Y(pb[0]), 1.5, fill=0 if "side" in lit else 255, outline=0, width=1.4)
    dw = DEVICE["right"]["dial_window"]["y"]
    c.rrect(X(bw) - 3, Y(dw[1]), X(bw) + 3.5, Y(dw[0]), 2.5, fill=255, outline=0, width=1.4)
    if rocker_marks:
        my = (Y(dw[1]) + Y(dw[0])) / 2
        for dy in (-7, 0, 7):
            c.rect(X(bw) + 0.5, my + dy - 0.6, X(bw) + 2.2, my + dy + 0.6)
    c.rrect(X(0), Y(bh), X(bw), Y(0), DEVICE["body"]["outline_corner_radius"] * scale, fill=255, outline=0, width=2)
    sw = DEVICE["front"]["screen_window"]
    c.rrect(X(sw["x"][0]), Y(sw["y"][1]), X(sw["x"][1]), Y(sw["y"][0]), 2, fill=255, outline=0, width=1.4)
    return {"top": ((X(tb[0]) + X(tb[1])) / 2, Y(bh) - 4),
            "side": (X(bw) + 4, (Y(pb[0]) + Y(pb[1])) / 2),
            "rocker": (X(bw) + 3.5, (Y(dw[0]) + Y(dw[1])) / 2),
            "screen": ((X(sw["x"][0]) + X(sw["x"][1])) / 2, (Y(sw["y"][0]) + Y(sw["y"][1])) / 2)}


def label(c, s, x, y, size=13, anchor="la", weight="ExtraBold"):
    c.text(s, nunito(size, weight), x, y, anchor=anchor)


# --------------------------------------------------------------------------- the illustrations
def timing_buttons(c):
    at = device(c, 24, 55, 1.36, lit=("top", "side"))
    sx, sy = at["screen"]
    stopwatch(c, sx, sy + 2, 9)
    tx, ty = at["top"]
    label(c, "Top", 128, 47)
    c.arrow((124, 55), (106, 55), (tx + 14, ty - 1), (tx + 6, ty + 2), width=2.4, head=7)
    label(c, "Rocker", 128, 75, size=12, weight="Bold")
    c.line([(124, 83), (at["rocker"][0] + 5, at["rocker"][1])], 1.2)
    label(c, "Side", 128, 106)
    c.arrow((124, 114), (110, 116), (104, at["side"][1]), (at["side"][0] + 3, at["side"][1]), width=2.4, head=7)


def time_a_stone(c):
    # the sheet: back line and tee (the house round it), the two hogs; the stone heading down the ice.
    # The first press is at the back line or the tee, your choice (1 or 1); then one at each hog line.
    back, tee, near, far = 14, 52, 108, 180
    label(c, "Back", 6, 45, size=12)
    label(c, "Tee", tee, 45, size=12, anchor="ma")
    label(c, "Near hog", near, 45, size=12, anchor="ma")
    label(c, "Far hog", 194, 45, size=12, anchor="ra")
    c.rrect(6, 66, 194, 84, 9, fill=None, outline=0, width=1.4)
    c.ring(tee, 75, 8, width=1.3)                       # the house
    c.ring(tee, 75, 3.5, width=1.3)
    for x in (back, tee, near, far):                    # the lines across the ice
        c.rect(x - 1, 60, x + 1, 90)
    for dx in (0, 6, 12):                               # the stone, moving down the ice
        c.line([(124 + dx, 75), (128 + dx, 75)], 1.4)
    c.disc(150, 75, 6.5)
    c.rrect(147.5, 66.8, 152.5, 69.5, 1, fill=0, outline=None)
    badge(c, back, 99, "1", r=7.5)
    label(c, "or", (back + tee) / 2, 92, size=11, anchor="ma", weight="Bold")
    badge(c, tee, 99, "1", r=7.5)
    badge(c, near, 99, "2", r=7.5)
    badge(c, far, 99, "3", r=7.5)
    f = nunito(12, "ExtraBold")
    pill(c, back + 4, 110, near - 11, 128, "B2H", f, black=False, r=9)
    pill(c, near + 11, 110, far - 11, 128, "H2H", f, black=True, r=9)

def read_the_times(c):
    # the top of the timing screen, as drawn on the device, and what each half is
    x0, x1, mid, y0, y1 = 8, 192, 90, 48, 92
    c.rrect(x0, y0, x1, y1, 4, fill=255, outline=0, width=1.6)
    c.rect(mid, y0 + 0.8, x1 - 0.8, y1 - 0.8)
    c.rrect(mid, y0, x1, y1, 4, fill=None, outline=0, width=1.6)
    c.digits = (("3.41", 3.41, x0, mid, 0), ("13.86", 13.86, mid, x1, 1))
    label(c, "Back to hog", (x0 + mid) / 2, 106, size=12, anchor="ma")
    label(c, "Hog to hog", (mid + x1) / 2, 106, size=12, anchor="ma")
    c.arrow(((x0 + mid) / 2, 104), ((x0 + mid) / 2, 100), ((x0 + mid) / 2, 98), ((x0 + mid) / 2, 94.5), width=2, head=6)
    c.arrow(((mid + x1) / 2, 104), ((mid + x1) / 2, 100), ((mid + x1) / 2, 98), ((mid + x1) / 2, 94.5), width=2, head=6)
    label(c, "(white)", (x0 + mid) / 2, 119, size=11, anchor="ma", weight="Bold")
    label(c, "(black)", (mid + x1) / 2, 119, size=11, anchor="ma", weight="Bold")


def columns(c, names, lit, x1=150, y=50):
    n = len(names)
    x0 = 8
    w = (x1 - x0) / n
    f = nunito(11, "ExtraBold")
    for i, name in enumerate(names):
        a = x0 + i * w
        pill(c, a + 2, y, a + w - 2, y + 20, name, f, black=i == lit, r=5)
    return [(x0 + i * w + w / 2) for i in range(n)]


def whose_turn(c):
    xs = columns(c, ("3rd", "Skip", "THEM"), lit=1, x1=142)
    stopwatch(c, xs[1], 102, 11)                              # the next time lands here
    c.arrow((xs[1], 84), (xs[1], 81), (xs[1], 78), (xs[1], 74), width=2, head=6)
    rocker(c, 168, 46, lit=("up", "down"))
    label(c, "left", 164, 50, size=11, anchor="ra", weight="Bold")
    label(c, "right", 164, 94, size=11, anchor="ra", weight="Bold")


def move_time(c):
    xs = columns(c, ("3rd", "Skip"), lit=0, x1=124)
    # the newest time, filed under Skip, moving one column left while up is held
    time_pill(c, xs[1] - 18, 80, xs[1] + 18, 98, "3.42", False)
    c.rrect(xs[0] - 18, 80, xs[0] + 18, 98, 5, fill=None, outline=0, width=1.2)  # where it goes
    for dx in range(-14, 15, 6):                                                    # (dashed)
        c.rect(xs[0] + dx, 79, xs[0] + dx + 2.6, 81, fill=255)
        c.rect(xs[0] + dx, 97, xs[0] + dx + 2.6, 99, fill=255)
    c.arrow((xs[1], 101), (xs[1] - 4, 122), (xs[0] + 4, 122), (xs[0], 101.5), width=2.4, head=7)
    rocker(c, 164, 46, lit=("up",), hold=("up", "left"))


def older_times(c, basic=False):
    # rows of times with the latest at the top, the list moving down a row
    rows = (("3.43", "13.91"), ("3.56", "14.12"), ("3.42", "13.60"))
    for i, (a, b) in enumerate(rows):
        y = 50 + i * 26
        time_pill(c, 14, y, 62, y + 20, a, False)
        time_pill(c, 66, y, 120, y + 20, b, True)
    if basic:
        rocker(c, 158, 46, lit=("up", "down"))
        label(c, "newer", 154, 52, size=11, anchor="ra", weight="Bold")
        label(c, "older", 154, 94, size=11, anchor="ra", weight="Bold")
    else:
        rocker(c, 158, 44, lit=("down",), hold=("down", "below"), scale=0.92)
        c.arrow((132, 58), (140, 74), (140, 90), (132, 106), width=2.2, head=7)


def log_shot(c):
    rocker(c, 88, 46, lit=("up", "down"))
    rock(c, 36, 58, "made", r=11)
    label(c, "Made", 36, 74, size=12, anchor="ma")
    rock(c, 164, 98, "missed", r=11)
    label(c, "Missed", 164, 114, size=12, anchor="ma")
    c.arrow((86, 57), (70, 57), (60, 57), (51, 58), width=2.2, head=7)
    c.arrow((114, 101), (130, 101), (140, 99), (149, 98), width=2.2, head=7)


def position(c):
    rocker(c, 12, 46, lit=("middle",))
    badge(c, 50, 79, "2x", r=10)
    f = nunito(12, "ExtraBold")
    # the four positions round a loop, clockwise: Lead, 2nd, 3rd, Skip
    spots = {"Lead": (98, 56), "2nd": (162, 56), "3rd": (162, 112), "Skip": (98, 112)}
    for name, (cx, cy) in spots.items():
        pill(c, cx - 22, cy - 10, cx + 22, cy + 10, name, f, black=name == "Lead", r=10)
    c.arrow((122, 56), (130, 56), (134, 56), (139, 56), width=2, head=6)
    c.arrow((162, 68), (162, 80), (162, 90), (162, 100), width=2, head=6)
    c.arrow((139, 112), (130, 112), (126, 112), (121, 112), width=2, head=6)
    c.arrow((98, 100), (98, 90), (98, 80), (98, 68), width=2, head=6)


def coach_pages(c):
    rocker(c, 12, 46, lit=("middle",))
    badge(c, 50, 79, "2x", r=10)
    f = nunito(12, "ExtraBold")
    for x, name, page in ((68, "Times", 0), (142, "Rocks", 1)):
        c.rrect(x, 52, x + 50, 110, 6, fill=255, outline=0, width=1.8)
        for k in range(2):  # the page marks
            mx = x + 35 + k * 7
            c.rrect(mx, 56, mx + 4.5, 60.5, 0.5, fill=0 if k == page else 255, outline=0, width=1)
        c.ctext(name, f, x + 25, 75)
    c.arrow((121, 72), (126, 64), (134, 64), (139, 72), width=2, head=6)
    c.arrow((139, 90), (134, 98), (126, 98), (121, 90), width=2, head=6)


def undo(c):
    rocker(c, 34, 46, lit=("middle",))
    badge(c, 74, 79, "1s", r=11)
    # the undo mark: a counter-clockwise arc ending in a head, then the time it takes away
    cx, cy, r = 112, 79, 12
    pts = [(cx + r * math.cos(math.radians(a)), cy - r * math.sin(math.radians(a))) for a in range(-40, 181, 5)]
    c.line(pts[:-2], 2.4)
    end = pts[-1]
    c.poly([(end[0] - 6.5, end[1] - 2), (end[0] + 6.5, end[1] - 2), (end[0], end[1] + 6)], 0)
    c.rrect(136, 70, 190, 90, 5, fill=255, outline=0, width=1.3)            # the time, going
    for dx in range(2, 52, 6):
        c.rect(136 + dx, 69, 136 + dx + 2.6, 71, fill=255)
        c.rect(136 + dx, 89, 136 + dx + 2.6, 91, fill=255)
    c.ctext("13.86", nunito(13, "ExtraBold"), 163, 72)
    label(c, "Keep holding to undo more", 100, 116, size=11, anchor="ma", weight="Bold")


def turn_off(c):
    at = device(c, 24, 55, 1.36, lit=("side",))
    sx, sy = at["screen"]
    # a power symbol on the screen: a ring with a gap at the top, and a bar through it
    pts = [(sx + 9 * math.cos(math.radians(a)), sy - 9 * math.sin(math.radians(a))) for a in range(125, 416, 5)]
    c.line(pts, 2.2)
    c.line([(sx, sy - 12), (sx, sy - 2)], 2.2)
    label(c, "Side", 128, 98)
    c.arrow((124, 106), (110, 108), (104, at["side"][1]), (at["side"][0] + 3, at["side"][1]), width=2.4, head=7)
    badge(c, 172, 106, "3s", r=12)


def skip_menu(c):
    # the rocker held as the side button switches it on: which mode each part opens
    rocker(c, 34, 46, lit=("up", "middle", "down"))
    for i, name in enumerate(("Coach", "Competition", "League")):
        cy = 46 + 11 + i * 22
        c.arrow((64, cy), (70, cy), (74, cy), (80, cy), width=2, head=6)
        label(c, name, 86, cy - 8, size=13)
    label(c, "Basic: from the menu", 100, 118, size=11, anchor="ma", weight="Bold")


def hog_to_hog(c):
    # a stone timed from the near hog: no B2H comes, so after 7s the watch moves to the H2H half
    for x0, name, half in ((8, "Start", 0), (112, "After 7s", 1)):
        x1, mid, y0, y1 = x0 + 80, x0 + 36, 62, 98
        c.rrect(x0, y0, x1, y1, 3, fill=255, outline=0, width=1.6)
        c.rect(mid, y0 + 0.8, x1 - 0.8, y1 - 0.8)
        c.rrect(x0, y0, x1, y1, 3, fill=None, outline=0, width=1.6)
        cx = (x0 + mid) / 2 if half == 0 else (mid + x1) / 2
        stopwatch(c, cx, (y0 + y1) / 2 + 2, 9, fill=0 if half == 0 else 255)
        label(c, name, (x0 + x1) / 2, 45, size=12, anchor="ma")
    c.arrow((92, 80), (98, 80), (102, 80), (108, 80), width=2.2, head=6)
    label(c, "B2H", 26, 103, size=11, anchor="ma", weight="Bold")
    label(c, "H2H", 172, 103, size=11, anchor="ma", weight="Bold")


# --------------------------------------------------------------------------- the cards
CARDS = {
    "buttons": ("Timing buttons", timing_buttons, ("Press the top or side", "button at each line.")),
    "stone": ("Time a stone", time_a_stone, ("Press at back or tee line,", "then at each hog line.")),
    "read": ("Read the times", read_the_times, ("The big numbers show the", "last stone you timed.")),
    "turn": ("Who's throwing", whose_turn, ("The highlight moves by", "itself, or use the rocker.")),
    "fix": ("Wrong player?", move_time, ("Hold up to move the", "last time one player left.")),
    "rocks": ("Log your shot", log_shot, ("Hold down again for rocks:", "Up: made. Down: missed.")),
    "older": ("Older times", older_times, ("Hold down to see older", "times. Hold up to return.")),
    "basic_older": ("Older stones", lambda c: older_times(c, basic=True), ("Down: older stones.", "Up: newer stones.")),
    "position": ("Your position", position, ("Double-click the middle", "to change your position.")),
    "pages": ("Two pages", coach_pages, ("Double-click the middle", "to switch times and rocks.")),
    "undo": ("Undo", undo, ("Hold the middle 1 second", "to undo a time or shot.")),
    "undo_times": ("Undo", undo, ("Hold the middle 1 second", "to undo the last time.")),
    "off": ("Turn off", turn_off, ("Hold the side button", "3 seconds to turn off.")),
    "menu": ("Skip the menu", skip_menu, ("Hold the rocker as you", "turn it on to skip the menu.")),
    "h2h": ("Hog to hog only", hog_to_hog, ("Start at the near hog?", "After 7s it times H2H.")),
}
DECKS = {
    "Basic": ("buttons", "stone", "h2h", "read", "basic_older", "undo_times", "off", "menu"),
    "Competition": ("buttons", "stone", "h2h", "read", "turn", "fix", "older", "position", "undo_times", "off",
                    "menu"),
    # Older times (hold down once) before Log your shot (hold down again): the order the holds go in
    "League": ("buttons", "stone", "h2h", "read", "turn", "fix", "older", "rocks", "position", "undo", "off", "menu"),
    "Coach": ("buttons", "stone", "h2h", "read", "turn", "fix", "older", "rocks", "pages", "undo", "off", "menu"),
}


def header(c, mode, index, count, title):
    label(c, mode, 10, 2, size=12, weight="Bold")
    label(c, f"{index + 1}/{count}", 190, 2, size=12, anchor="ra", weight="Bold")
    f = nunito(19, "ExtraBold")
    assert width_of(title, f) <= 184, title
    c.ctext(title, f, 100, TITLE_Y)
    c.rect(10, RULE_Y, 190, RULE_Y + 0.8)


def footer(c, index, count):
    c.rect(0, FOOT_Y, W, H)
    f = nunito(13, "Bold")
    y = FOOT_Y + 4
    if index > 0:  # up: back
        c.poly([(14, y + 4), (9, y + 12), (19, y + 12)], 255)
        c.text("Back", f, 23, y, fill=1)
    last = index == count - 1
    c.disc(100 - width_of("Done", f) / 2 - 8, y + 8.5, 3.2, 255)
    c.ctext("Done", f, 100 + 4, y, fill=1)
    if not last:  # down: next
        c.text("Next", f, 168, y, fill=1, anchor="ra")
        c.poly([(186, y + 12), (181, y + 4), (191, y + 4)], 255)


def copy_lines(c, lines):
    f = nunito(15, "Bold")
    for line, y in zip(lines, COPY_Y):
        assert width_of(line, f) <= 192, line
        c.ctext(line, f, 100, y)


def build(mode, index):
    key = DECKS[mode][index]
    title, draw, lines = CARDS[key]
    c = Card()
    c.digits = ()
    header(c, mode, index, len(DECKS[mode]), title)
    draw(c)
    copy_lines(c, lines)
    footer(c, index, len(DECKS[mode]))
    img = c.finish()
    if c.digits:  # DSEG digits drawn as on the device, after the reduction
        d = ImageDraw.Draw(img)
        d.fontmode = "1"
        font = ImageFont.truetype(str(screen.DSEG), 26)
        for _, value, a, b, white in c.digits:
            screen.dseg_number(d, value, font, a, b, 48, 92, margin=0, align="center", fill=white)
    return img


def main():
    cards, sheets = HERE / "cards", HERE / "sheets"
    cards.mkdir(exist_ok=True)
    sheets.mkdir(exist_ok=True)
    z = 3
    for mode, deck in DECKS.items():
        imgs = [build(mode, i) for i in range(len(deck))]
        for i, img in enumerate(imgs):
            img.save(cards / f"{mode.lower()}-{i + 1:02d}-{deck[i]}.png")
        per_row = 4
        rows = (len(imgs) + per_row - 1) // per_row
        sheet = Image.new("RGB", (per_row * (W * z + 20) + 20, rows * (H * z + 20) + 20), (246, 246, 244))
        for i, img in enumerate(imgs):
            sheet.paste(screen.to_rgb(img, z), (20 + (i % per_row) * (W * z + 20), 20 + (i // per_row) * (H * z + 20)))
        sheet.save(sheets / f"{mode.lower()}.png")
        print("wrote", mode, len(imgs), "cards")


if __name__ == "__main__":
    main()
