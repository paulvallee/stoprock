"""Concept sheet: a two-part TPU case for stoprock on the M5Stack CoreInk (dimensions approximate).

    python tools/mockups/case_concept.py   -> docs/mockups/case-concept.png
"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from PIL import Image, ImageDraw, ImageFont  # noqa: E402
import logo  # noqa: E402
import render  # noqa: E402
import screen  # noqa: E402

SS = 3
SHEET = (2200, 1340)
BG = (246, 246, 244)
img = Image.new("RGB", (SHEET[0] * SS, SHEET[1] * SS), BG)
d = ImageDraw.Draw(img)

TUB = (58, 61, 66)
BEZEL = (84, 88, 95)
PAD = (232, 105, 46)
PAD_HI = (255, 228, 210)
EDGE = (28, 29, 32)
DEV = (236, 236, 232)
INK = (40, 40, 40)
NOTE = (100, 100, 100)
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def font(size, bold=False):
    return ImageFont.truetype(BOLD if bold else REG, size * SS)


def text(x, y, t, size=15, bold=False, fill=INK, anchor="la"):
    d.text((x * SS, y * SS), t, font=font(size, bold), fill=fill, anchor=anchor)


def badge(x, y, n):
    """A numbered marker at sheet (x, y)."""
    r = 13
    d.ellipse([(x - r) * SS, (y - r) * SS, (x + r) * SS, (y + r) * SS], fill=(25, 90, 170), outline=(255, 255, 255), width=2 * SS)
    d.text((x * SS, y * SS), str(n), font=font(14, True), fill=(255, 255, 255), anchor="mm")


class View:
    """Millimetre coordinates mapped to the sheet: origin (ox, oy), k sheet px per mm."""
    def __init__(self, ox, oy, k):
        self.ox, self.oy, self.k = ox, oy, k

    def x(self, v):
        return (self.ox + v * self.k) * SS

    def y(self, v):
        return (self.oy + v * self.k) * SS

    def p(self, vx, vy):  # sheet px (for badges)
        return self.ox + vx * self.k, self.oy + vy * self.k

    def rr(self, x0, y0, x1, y1, r, fill, outline=EDGE, w=2):
        d.rounded_rectangle([self.x(x0), self.y(y0), self.x(x1), self.y(y1)], radius=int(r * self.k * SS),
                            fill=fill, outline=outline, width=w * SS)

    def ell(self, x0, y0, x1, y1, fill, outline=EDGE, w=2):
        d.ellipse([self.x(x0), self.y(y0), self.x(x1), self.y(y1)], fill=fill, outline=outline, width=w * SS)


DW, DH, DT = 40, 56, 16           # CoreInk, approximately
WALL = 2.5
CW, CH, CT = DW + 2 * WALL, DH + 2 * WALL, DT + 2 * WALL
SCR, SCR_TOP = 30, 8
K = 9

text(60, 48, "stoprock: two-part TPU case (concept)", 30, True)
text(60, 92, "M5Stack CoreInk inside. Dimensions and button positions approximate (CoreInk about 40 x 56 x 16 mm): "
     "to check against the device.", 15, False, NOTE)

# ------------------------------------------------------------------ front
v = View(150, 250, K)
text(110, 160, "Front", 22, True)
v.ell(-4.5, -4.5, 7, 7, TUB)                          # lanyard loop
v.ell(-1.8, -1.8, 3.2, 3.2, BG)
v.rr(CW / 2 - 7, -1.8, CW / 2 + 7, 2, 1.5, PAD)       # top button pad
v.rr(-1.6, 16, 1.5, 28, 1.5, PAD)                     # side button pad
v.rr(CW - 1.5, 12, CW + 2.2, 30, 2, PAD)              # rocker cover
cx = v.x(CW + 0.35)
for yy, up in ((15.2, True), (26.8, False)):
    a, b = (v.y(yy - 1), v.y(yy + 1)) if up else (v.y(yy + 1), v.y(yy - 1))
    d.polygon([(cx - 6 * SS, b), (cx + 6 * SS, b), (cx, a)], fill=PAD_HI)
d.ellipse([cx - 5 * SS, v.y(21) - 5 * SS, cx + 5 * SS, v.y(21) + 5 * SS], fill=PAD_HI)
v.rr(0, 0, CW, CH, 6, TUB)
v.rr(1.3, 1.3, CW - 1.3, CH - 1.3, 5, BEZEL)
sx0 = CW / 2 - SCR / 2
v.rr(sx0 - 0.9, SCR_TOP - 0.9, sx0 + SCR + 0.9, SCR_TOP + SCR + 0.9, 2, (44, 46, 50))
scr = screen.league_screen(render.LEAGUE_STONES, render.BASE + [False], render.LEAGUE_ME, render.LEAGUE_ACTIVE)
size = int(SCR * K * SS)
img.paste(screen.to_rgb(scr, 1).resize((size, size), Image.LANCZOS), (int(v.x(sx0)), int(v.y(SCR_TOP))))
wm = logo.wordmark().convert("L")
lw = int(22 * K * SS)
wm = wm.resize((lw, int(wm.height * lw / wm.width)), Image.LANCZOS)
img.paste(Image.new("RGB", wm.size, (150, 155, 162)), (int(v.x(CW / 2) - lw / 2), int(v.y(45.5))),
          wm.point(lambda q: 255 - q))
v.ell(CW - 7.6, 4.4, CW - 6.4, 5.6, (120, 200, 130), EDGE, 1)   # LED window
text(150 + CW * K / 2, 250 + CH * K + 30, f"about {CW:.0f} x {CH:.0f} x {CT:.0f} mm", 14, False, NOTE, "ma")
badge(*v.p(-4.5, -6), 1)
badge(*v.p(CW / 2 + 9, -3.5), 2)
badge(*v.p(-4.2, 22), 3)
badge(*v.p(CW + 4.8, 21), 4)
badge(*v.p(CW - 3.8, 5), 5)
badge(*v.p(CW / 2 + 13.5, 48), 6)

# ------------------------------------------------------------------ sides
def side(ox, title, which):
    s = View(ox, 250, K)
    text(ox, 160, title, 22, True)
    s.rr(0, 0, CT, CH, 4, TUB)
    s.rr(CT - 4, 0.6, CT, CH - 0.6, 3, BEZEL)
    d.line([s.x(CT - 4), s.y(3), s.x(CT - 4), s.y(CH - 3)], fill=(18, 18, 20), width=2 * SS)
    if which == "right":
        s.rr(CT / 2 - 3.5, 12, CT / 2 + 3.5, 30, 3, PAD)
        d.line([s.x(CT / 2), s.y(13.5), s.x(CT / 2), s.y(28.5)], fill=PAD_HI, width=2 * SS)
    else:
        s.rr(CT / 2 - 3, 16, CT / 2 + 3, 28, 3, PAD)
        s.rr(CT / 2 - 4.2, 14.8, CT / 2 + 4.2, 29.2, 3.5, None, (110, 114, 120), 2)   # guard rim
    text(ox + CT * K / 2, 250 + CH * K + 30, "back   |   front", 13, False, NOTE, "ma")
    return s


s1 = side(740, "Right side", "right")
badge(*s1.p(CT / 2, 9), 4)
badge(*s1.p(CT + 2.6, 40), 7)
s2 = side(1000, "Left side", "left")
badge(*s2.p(CT / 2, 12), 3)

# ------------------------------------------------------------------ bottom
b = View(150, 1000, K)
text(110, 935, "Bottom", 22, True)
b.rr(0, 0, CW, CT, 4, TUB)
b.rr(0.6, CT - 4, CW - 0.6, CT, 3, BEZEL)
b.rr(CW / 2 - 5.5, CT / 2 - 3, CW / 2 + 5.5, CT / 2 + 1, 1.5, (22, 22, 24))
text(150 + CW * K / 2, 1000 + CT * K + 26, "back  (top of drawing)   |   front", 13, False, NOTE, "ma")
badge(*b.p(CW / 2 + 9.5, CT / 2 - 1), 8)

# ------------------------------------------------------------------ exploded
e = View(1270, 250, K)
text(1250, 160, "Exploded, from the side", 22, True)
e.rr(0, 0, 9, CH, 4, TUB)
d.rectangle([e.x(2.5), e.y(2.5), e.x(9.3), e.y(CH - 2.5)], fill=BG)
e.ell(-0.6, -4.5, 5, 1.5, TUB)
e.ell(1.0, -3, 3.4, 0, BG)
d.ellipse([e.x(-0.8), e.y(40) - 4 * SS, e.x(0.6), e.y(40) + 4 * SS], fill=(15, 15, 15))
e.rr(14, WALL, 14 + DT, WALL + DH, 2, DEV, (120, 120, 120))
d.rectangle([e.x(14 + DT - 1), e.y(WALL + 6), e.x(14 + DT), e.y(WALL + 36)], fill=(70, 70, 70))
d.rectangle([e.x(34.8), e.y(1.6), e.x(36.4), e.y(CH - 1.6)], fill=BEZEL, outline=EDGE, width=SS)
e.rr(36, 0, 40, CH, 3, BEZEL)
for lx, label, sub in ((4.5, "Tub", "back + walls"), (14 + DT / 2, "CoreInk", "slides in"), (38, "Bezel", "screen frame")):
    text(1270 + lx * K, 250 + CH * K + 26, label, 16, True, INK, "ma")
    text(1270 + lx * K, 250 + CH * K + 48, sub, 13, False, NOTE, "ma")
for lx in (11, 33):
    text(1270 + lx * K, 250 + CH * K / 2, "→", 26, True, NOTE, "mm")
badge(*e.p(-3.6, 40), 9)
badge(*e.p(35.6, -3), 7)

# ------------------------------------------------------------------ button section
c = View(780, 1010, 15)
text(740, 935, "A button, in section", 22, True)
c.rr(0, 0, 9, 2.5, 0.3, TUB, TUB, 1)
c.rr(21, 0, 30, 2.5, 0.3, TUB, TUB, 1)
d.rectangle([c.x(9), c.y(1.6), c.x(21), c.y(2.5)], fill=TUB)
c.rr(10, -1.5, 20, 1.6, 1.4, PAD, PAD, 1)
d.rectangle([c.x(14), c.y(2.5), c.x(16), c.y(3.6)], fill=PAD)
d.rectangle([c.x(5), c.y(3.75), c.x(25), c.y(6.5)], fill=DEV, outline=(120, 120, 120), width=SS)
d.rectangle([c.x(13), c.y(3.75), c.x(17), c.y(4.7)], fill=(90, 90, 90))
text(780 + 31 * 15, 1010 + 0.7 * 15, "case wall", 13, False, NOTE)
text(780 + 26 * 15, 1010 + 5 * 15, "CoreInk and its button", 13, False, NOTE)
badge(*c.p(15, -3.2), 10)

# ------------------------------------------------------------------ key
kx, ky = 1700, 175
text(kx, ky - 15, "Key", 22, True)
items = [
    ("Lanyard loop", "Moulded into the tub, 3.5 mm thick, rounded. No grommets."),
    ("Top button: timing", "Raised pad 14 x 4 mm, 1.5 mm proud, textured."),
    ("Side button: timing; hold 3 s = power", "Pad on the tub's left wall, with a low rim round it against pocket presses."),
    ("Rocker cover", "Soft cover tilting on a rib behind its middle: up / press / down. Ridges and a centre dimple. Fallback: an open cut-out."),
    ("LED window", "Thin TPU over the green LED, or a clear insert."),
    ("Wordmark", "Debossed in the bezel."),
    ("Two parts", "The bezel's lip snaps into a groove in the tub's rim (or 4 small screws into bosses)."),
    ("USB-C opening", "For charging; optional plug-in TPU flap for wet ice."),
    ("Reset pinhole", "Recessed in the back: reach it with a pin, never by accident."),
    ("How a pad presses", "Pad on a 0.6-1 mm web that bends and springs back; a nub inside sits 0.1-0.2 mm from the switch (tune by test print)."),
]


def wrap(t, n):
    words, lines, cur = t.split(), [], ""
    for w in words:
        if len(cur) + len(w) + 1 > n:
            lines.append(cur)
            cur = w
        else:
            cur = (cur + " " + w).strip()
    return lines + [cur]


y = ky + 30
for i, (title, body) in enumerate(items, 1):
    badge(kx + 13, y + 9, i)
    text(kx + 36, y, title, 15, True)
    y += 21
    for line in wrap(body, 46):
        text(kx + 36, y, line, 13, False, NOTE)
        y += 18
    y += 12

# legend
for i, (col, label) in enumerate(((TUB, "Tub: TPU (95A), charcoal"), (BEZEL, "Bezel: TPU (95A)"),
                                  (PAD, "Button pads, a colour you can find by eye"))):
    x = 60 + i * 420
    d.rectangle([x * SS, 1290 * SS, (x + 26) * SS, 1310 * SS], fill=col, outline=EDGE, width=SS)
    text(x + 36, 1291, label, 15)

out = screen.ROOT / "docs" / "mockups" / "case-concept.png"
img.resize(SHEET, Image.LANCZOS).save(out)
print("wrote", out.relative_to(screen.ROOT))
