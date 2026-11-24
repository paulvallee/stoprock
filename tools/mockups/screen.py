"""Pixel-exact mockups of the stoprock screens (200 x 200, 1-bit), as agreed in docs/SPEC.md."""
from pathlib import Path
import tempfile

from PIL import BdfFontFile, Image, ImageChops, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
FONTS = ROOT / "assets" / "fonts"
DSEG = FONTS / "DSEG7Classic-Bold.ttf"
OFF_PAGE_PATH = ROOT / "assets" / "screens" / "wake-screen-200x200-1bit.png"

W = H = 200
TIP_H = 26  # competition's badge strip

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


def text_in_box(d, s, font, x0, x1, y0, y1, align="right", margin=4, fill=0):
    if isinstance(font, SegFont):
        return font.draw_in_box(d, s, x0, x1, y0, y1, align, margin)
    l, t, r, b = ink(s, font)
    w = r - l
    x = {"right": x1 - margin - w - l, "left": x0 + margin - l}.get(align, (x0 + x1 - w) / 2 - l)
    y = (y0 + y1 - (b - t)) / 2 - t
    d.text((round(x), round(y)), s, font=font, fill=fill)


def dseg_number(d, val, font, x0, x1, y0, y1, margin=3, align="right", fill=0):
    """DSEG7 number with a small square decimal point, right-aligned (or centred) in the box."""
    ip, fp = f"{val:.2f}".split(".")
    li, ti, ri, bi = ink(ip, font)
    lf, tf, rf, bf = ink(fp, font)
    t, b = min(ti, tf), max(bi, bf)
    dot, gap = max(3, round((b - t) * 0.13)), 2
    total = (ri - li) + gap + dot + gap + (rf - lf)
    x = x1 - margin - total if align == "right" else (x0 + x1 - total) / 2
    y = (y0 + y1 - (b - t)) / 2 - t
    glyphs(d, ip, font, round(x - li), round(y), fill)
    xd = x + (ri - li) + gap
    yb = y + b
    d.rectangle([round(xd), round(yb - dot), round(xd + dot - 1), round(yb - 1)], fill=fill)
    glyphs(d, fp, font, round(xd + dot + gap - lf), round(y), fill)


def glyphs(d, s, font, x, y, fill=0):
    """Draw s one glyph at a time, each at a whole-pixel advance: as the firmware does.
    (Pillow lays a string out at fractional advances, which can shift a glyph by a pixel.)"""
    for ch in s:
        d.text((x, y), ch, font=font, fill=fill)
        x += round(font.getlength(ch))


# --------------------------------------------------------------------------- drawn segments
SEGMENTS = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc",
            "5": "afgcd", "6": "afgedc", "7": "abc", "8": "abcdefg", "9": "abcdfg", "-": "g"}


class SegFont:
    """Seven-segment digits drawn from rectangles, with the corner pixels left empty."""

    def __init__(self, h, w, t, gap, dot=None, prop=False, solid_one=False):
        self.h, self.w, self.t, self.gap, self.prop = h, w, t, gap, prop
        self.dot = dot or t
        self.solid_one = solid_one  # "1" as one stroke: at big sizes the gap between its two reads as "!"

    def _width(self, ch):
        if ch == ".":
            return self.dot
        if ch == "1" and self.prop:
            return self.t
        return self.w

    def width(self, s):
        return sum(self._width(c) for c in s) + self.gap * (len(s) - 1)

    def _digit(self, d, ch, x, y, fill=0):
        w, h, t = self.w, self.h, self.t
        segs = SEGMENTS.get(ch, "")
        m = (h - t) // 2
        c = 1 if t == 1 else t - 1
        extra = 1 if t == 1 else 0

        def r(x0, y0, x1, y1):
            d.rectangle([x + x0, y + y0, x + x1 - 1, y + y1 - 1], fill=fill)

        if ch == "1" and self.solid_one:
            r(w - t, c, w, h - c)
            return
        if "a" in segs: r(c, 0, w - c, t)
        if "g" in segs: r(c, m, w - c, m + t)
        if "d" in segs: r(c, h - t, w - c, h)
        if "f" in segs: r(0, c, t, m + t - c + extra)
        if "b" in segs: r(w - t, c, w, m + t - c + extra)
        if "e" in segs: r(0, m + c, t, h - c)
        if "c" in segs: r(w - t, m + c, w, h - c)

    def draw(self, d, s, x, y, fill=0):
        for ch in s:
            if ch == ".":
                d.rectangle([x, y + self.h - self.dot, x + self.dot - 1, y + self.h - 1], fill=fill)
            elif ch == "1" and self.prop:
                self._digit(d, ch, x - (self.w - self.t), y, fill)
            else:
                self._digit(d, ch, x, y, fill)
            x += self._width(ch) + self.gap

    def draw_in_box(self, d, s, x0, x1, y0, y1, align="right", margin=4, fill=0):
        w = self.width(s)
        x = {"right": x1 - margin - w, "left": x0 + margin}.get(align, (x0 + x1 - w) // 2)
        self.draw(d, s, x, (y0 + y1 - self.h) // 2, fill)


_fonts = None


def fonts():
    """Created once: ink() caches measurements by font object, so fonts must not be recreated
    (a new font object can reuse an old one's id and pick up its measurements)."""
    global _fonts
    if _fonts is None:
        _fonts = _make_fonts()
    return _fonts


def _make_fonts():
    from logo import NUNITO

    menu_title = ImageFont.truetype(str(NUNITO), 18)
    menu_title.set_variation_by_name("ExtraBold")
    menu_detail = ImageFont.truetype(str(NUNITO), 13)
    menu_detail.set_variation_by_name("Bold")
    return dict(
        menu_title=menu_title,
        menu_detail=menu_detail,
        label=bdf("spleen-6x12"),
        menu=bdf("spleen-16x32"),  # the switch-on tip
        hint=bdf("spleen-12x24"),  # the hold hints
        big=ImageFont.truetype(str(DSEG), 37),  # the newest stone, at the top
        big_long=ImageFont.truetype(str(DSEG), 33),  # ... an H2H of 20.00 s, too wide at 37
        top_small=ImageFont.truetype(str(DSEG), 24),  # coach page 2's newest stone
        row=SegFont(h=12, w=7, t=2, gap=2, dot=2),
        tiny=SegFont(h=9, w=5, t=1, gap=1),  # equal-width cells: a "1" sits on the right
        narrow=SegFont(h=12, w=6, t=2, gap=1, dot=2, prop=True),  # coach's five columns: a narrow "1"
        # the cursor's column, zoomed (zoom_columns()): the biggest digits 4 lines fit in each mode's
        # column height and 1.3 columns' width (a white-on-black cell keeping 1 px of black round
        # them), a narrow solid "1"; "_long": narrower, for 20.00, the one time with no "1" to spare.
        zoom_league=SegFont(h=22, w=13, t=3, gap=2, dot=3, prop=True, solid_one=True),
        zoom_league_long=SegFont(h=22, w=11, t=3, gap=2, dot=3, prop=True, solid_one=True),
        zoom_comp=SegFont(h=24, w=14, t=3, gap=2, dot=3, prop=True, solid_one=True),
        zoom_comp_long=SegFont(h=24, w=11, t=3, gap=2, dot=3, prop=True, solid_one=True),
        zoom_coach=SegFont(h=18, w=11, t=2, gap=2, dot=2, prop=True, solid_one=True),
        zoom_coach_long=SegFont(h=18, w=8, t=2, gap=2, dot=2, prop=True, solid_one=True),
        pct=ImageFont.truetype(str(DSEG), 16),
    )


# --------------------------------------------------------------------------- percentage
def percent_of(made, n):
    """Rounded percentage, half up (as the firmware rounds; Python's round() rounds half to even)."""
    return (made * 200 + n) // (2 * n)


def big_percent(d, f, pct, x0, x1, y0, y1):
    """DSEG7 digits packed by their ink and a % sign drawn to match, centred in the box."""
    n = str(pct)
    boxes = [ink(c, f) for c in n]
    t, b = min(bx[1] for bx in boxes), max(bx[3] for bx in boxes)
    dh = b - t
    pw, st, q = round(dh * 0.6), 2, 3
    gap = 2 if len(n) > 2 else 3
    total = sum(bx[2] - bx[0] for bx in boxes) + gap * len(n) + pw
    x = (x0 + x1 - total) / 2
    ytop = (y0 + y1 - dh) / 2
    for c, bx in zip(n, boxes):
        d.text((round(x - bx[0]), round(ytop - t)), c, font=f, fill=0)
        x += (bx[2] - bx[0]) + gap
    x, y = round(x), round(ytop)
    d.rectangle([x, y, x + q - 1, y + q - 1], fill=0)
    d.rectangle([x + pw - q, y + dh - q, x + pw - 1, y + dh - 1], fill=0)
    d.line([(x + pw - 1, y), (x, y + dh - 1)], fill=0, width=st)


def draw_pie(d, cx, cy, pct):
    """11 px pie: a 1-px ring and, inside a 1-px gap, a wedge filled clockwise from 12 o'clock."""
    import math
    for dy in range(-6, 7):
        for dx in range(-6, 7):
            d2 = dx * dx + dy * dy
            if 20 < d2 <= 30:
                d.point((cx + dx, cy + dy), fill=0)
            elif d2 <= 13 and pct > 0:  # 1-px gap between wedge and ring
                a = math.atan2(dx, -dy)
                if a < 0:
                    a += 2 * math.pi
                if pct >= 100 or a < pct / 100 * 2 * math.pi:
                    d.point((cx + dx, cy + dy), fill=0)


MENU_ITEMS = (
    ("Basic", "Just the stopwatch"),
    ("Competition", "Times only, by player"),
    ("League", "Team times, your shots"),
    ("Coach", "Track the whole team"),
)
MENU_TOP, MENU_PITCH, MENU_HEIGHT, MENU_VISIBLE = 51, 44, 40, 3


def mode_menu(selected, first_visible=0):
    """The startup list below the fixed wordmark. The window follows the selected row."""
    from logo import NATIVE_LOGO

    f = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    with Image.open(NATIVE_LOGO) as logo:
        img.paste(logo, ((W - logo.width) // 2, 4))

    def card(left, top, right, bottom, fill):
        for row, inset in enumerate((3, 2, 1)):
            d.line((left + inset, top + row, right - inset, top + row), fill=fill)
            d.line((left + inset, bottom - row, right - inset, bottom - row), fill=fill)
        d.rectangle((left, top + 3, right, bottom - 3), fill=fill)

    count = len(MENU_ITEMS)
    first_visible = max(selected - MENU_VISIBLE + 1, min(first_visible, selected, count - MENU_VISIBLE))
    for row, (title, description) in enumerate(MENU_ITEMS[first_visible:first_visible + MENU_VISIBLE]):
        top = MENU_TOP + row * MENU_PITCH
        bottom = top + MENU_HEIGHT - 1
        on = row + first_visible == selected
        card(10, top, 184, bottom, 0)
        if not on:
            card(11, top + 1, 183, bottom - 1, 1)
        glyphs(d, title, f["menu_title"], 20, top + 3, int(on))
        glyphs(d, description, f["menu_detail"], 20, top + 22, int(on))
        if on:
            for offset in (0, 1):
                d.line((170 + offset, top + 15, 175 + offset, top + 20, 170 + offset, top + 25), fill=1)

    if count > MENU_VISIBLE:
        top = MENU_TOP + 2
        height = (MENU_VISIBLE - 1) * MENU_PITCH + MENU_HEIGHT - 4
        thumb = max(12, height * MENU_VISIBLE // count)
        offset = first_visible * (height - thumb) // (count - MENU_VISIBLE)
        d.line((192, top, 192, top + height - 1), fill=0)
        d.rectangle((190, top + offset, 194, top + offset + thumb - 1), fill=0)
    return img


def off_page(battery=-1, games_left=-1):
    with Image.open(OFF_PAGE_PATH) as image:
        img = image.copy()
    if battery < 0:
        return img
    battery = min(battery, 100)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    d.rectangle((12, 182, 27, 191), outline=0)
    d.rectangle((28, 185, 29, 188), fill=0)
    fill = (battery * 12 + 50) // 100
    if fill:
        d.rectangle((14, 184, 13 + fill, 189), fill=0)
    if games_left < 0:
        label = f"{battery}% charged"
    elif games_left == 0:
        label = "Charge soon"
    elif games_left == 1:
        label = "~1 game left"
    else:
        label = f"~{games_left} games left"
    glyphs(d, label, fonts()["menu_detail"], 36, 179)
    return img


# --------------------------------------------------------------------------- start-up modes
COACH_SLOTS = 64             # 8 ends x 4 positions x 2 rocks
POSITIONS = ("Lead", "2nd", "3rd", "Skip")
POSITION_SHORT = ("L", "2", "3", "S")
WIDE_MID = 100               # times split in competition and coach page 1
COACH_GX = [12 + k * 47 for k in range(5)]  # coach position columns


# Competition and league history: four columns, your three teammates in throwing order (you can't
# time your own rocks, so a lead sees 2nd, 3rd and Skip) and the other team, THEM. Each lists its
# stones newest first, a time a line; the newest stone overall is also shown large at the top. The
# cursor, the highlighted header, is on whoever throws next; it follows the throw order by itself,
# and the rocker moves it.
# H2H times are a digit wider than B2H, so the H2H half of the large row is wider: split at x 92.
# No labels: B2H is black on white, H2H white on black, as in the columns.
COMP_MID = 92
TIMES_Y1 = 44                # the newest stone large in y 0-44 (2 px line under it)
GRID_LABEL = (46, 57)        # player headers
GRID_PITCH = 14              # a line of the other columns' times
ZOOM_LINES = 4               # the zoomed column's lines, filling the column's height
ZOOM_SPREAD = 1.3            # ... and its width, in columns: over 15% of each neighbour (30% of one at an edge)
POSITION_NAMES = ("Lead", "2nd", "3rd", "Skip")


def teammates(me):
    """The three positions timed by a player at position me (0 lead .. 3 skip), in throwing order."""
    return [p for p in range(4) if p != me]


def stopwatch(d, cx, cy, r, fill=0):
    """A stopwatch: a 2 px ring of radius r, the crown on top, one hand at about 1 o'clock."""
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=fill, width=2)
    d.rectangle([cx - 1, cy - r - 2, cx + 1, cy - r - 1], fill=fill)   # stem
    d.rectangle([cx - 3, cy - r - 4, cx + 3, cy - r - 3], fill=fill)   # button
    d.line([(cx, cy), (cx + r // 2, cy - r // 2 - 1)], fill=fill, width=2)
    d.rectangle([cx - 1, cy - 1, cx + 1, cy + 1], fill=fill)           # centre


STOPWATCH_BIG, STOPWATCH_SMALL = 10, 6  # ring radius: in the large row, and coach page 2's


def newest_row(d, f, stones, waiting, mid, fonts_, y0, y1, r, cy):
    """The current stone across both halves, centred: B2H black on white, H2H white on its black
    half (filled by the caller). fonts_: (font, font for an H2H of 20.00 s, too wide for the first).
    waiting: "b2h", "h2h" or "h2h-only" while it's being timed (a stopwatch where that time will
    appear; for "b2h" and "h2h-only", a stone whose first click was 7 s ago with no second, so it
    can only be an H2H, the row is still empty)."""
    cur = None if waiting in ("b2h", "h2h-only") or not stones else stones[0]
    for k, (x0, x1) in enumerate(((0, mid), (mid, W))):
        fill = 1 if k else 0
        if cur is not None and cur[k] is not None:
            font = fonts_[1] if cur[k] >= 20 else fonts_[0]
            dseg_number(d, cur[k], font, x0, x1, y0, y1, margin=0, align="center", fill=fill)
        elif waiting == ("b2h", "h2h")[k] or (k == 1 and waiting == "h2h-only"):
            stopwatch(d, (x0 + x1) // 2, cy, r, fill)


# Tips, big: the switch-on tip (competition and league) says only your position, also shown when you
# change it. The hold hints say what holding does.
YOU_ARE = ("LEAD", "SECOND", "THIRD", "SKIP")
HINT_TOP = TIMES_Y1 + 2       # the hold hints: over the times
UNDO_TIP = ("Keep holding", "to undo")       # tip="undo": the rocker centre held, before the undo (1 s)
OFF_TIP = ("Keep holding", "to turn off")    # tip="off": the side button held, before it switches off (3 s)
PAGE_TIP = ("Double click", "to change page")  # tip="page": coach, the centre clicked once
MOVE_HINT = ("Moving time",)  # tip="move": the rocker held to move the newest time; one line, over the bottom
TIP_FONTS = {"hint": ("hint", 12, 24), "big": ("menu", 16, 32)}  # by size: the font, its advance and line height


YOU_ARE_TIP = ("You are", "2x to change")  # the switch-on tip's first line and its last (under the position)
ROCKER_Y = 59  # the rocker's height beside the screen (CoreInk: right side): the switch-on tip points at it


def you_are(me):
    return (YOU_ARE_TIP[0], YOU_ARE[me])


def you_are_tip(d, f, me):
    """The switch-on tip: "You are / SKIP" (bold Spleen 16x32) and "2x to change" (12x24) in a box on
    the left, level with the rocker, an arrow from it to the rocker (a double click changes position)."""
    lines = ((YOU_ARE_TIP[0], "big"), (YOU_ARE[me], "big"), (YOU_ARE_TIP[1], "hint"))
    w = max(TIP_FONTS[s][1] * len(text) for text, s in lines) + 1 + 16
    h = sum(TIP_FONTS[s][2] for _, s in lines) + 10
    x0, y0 = 2, ROCKER_Y - 11
    x1, y1 = x0 + w - 1, y0 + h - 1
    d.rectangle([x0, y0, x1, y1], fill=1, outline=0, width=2)
    y = y0 + 5
    for text, s in lines:
        key, cw, lh = TIP_FONTS[s]
        x = x0 + (w - (cw * len(text) + 1)) // 2
        for dx in (0, 1):
            d.text((x + dx, y), text, font=f[key], fill=0)
        y += lh
    arrow(d, x1 + 1, ROCKER_Y)


def arrow(d, x_from, y):
    """An arrow from x_from to the screen's right edge at y, pointing right, on a white band (so it
    reads over anything): a 5 px shaft and a head 9 px each side, drawn in columns."""
    tip, head = W - 1, 9
    d.rectangle([x_from, y - head - 2, tip, y + head + 2], fill=1)
    d.rectangle([x_from, y - 2, tip - head, y + 2], fill=0)
    for i in range(head + 1):
        d.rectangle([tip - i, y - i, tip - i, y + i], fill=0)


def tooltip(d, f, lines, top, bottom, size="hint"):
    """A bordered box (2 px) centred in top..bottom, its lines centred, bold (drawn twice, a pixel
    apart) in Spleen 12x24, or 16x32 for size "big"."""
    key, cw, lh = TIP_FONTS[size]
    w = min(cw * max(len(t) for t in lines) + 1 + 16, W - 12)
    h = lh * len(lines) + 10
    x0, y0 = (W - w) // 2, top + (bottom - top - h) // 2
    y1 = y0 + h - 1
    d.rectangle([x0, y0, x0 + w - 1, y1], fill=1, outline=0, width=2)
    y = y0 + 5
    for t in lines:
        x = (W - (cw * len(t) + 1)) // 2
        for dx in (0, 1):
            d.text((x + dx, y), t, font=f[key], fill=0)
        y += lh


def badge_strip(img, d, f, name, low_battery, note=None):
    """The bottom TIP_H px, white on black, for an umpire: the logo on the left, the mode's name and
    "STOPWATCH ONLY" (or "Battery low", or a note) right-aligned."""
    strip = H - TIP_H
    d.rectangle([0, strip, W - 1, H - 1], fill=0)
    from logo import strip_logo  # here, not at the top: logo.py is a sibling module
    lg = strip_logo()
    img.paste(1, (2, strip + (TIP_H - lg.height) // 2), ImageChops.invert(lg.convert("L")).convert("1"))
    sub = note or ("Battery low" if low_battery else "STOPWATCH ONLY")
    for t, y in ((name, strip + 1), (sub, strip + 13)):
        d.text((W - 2 - 6 * len(t), y), t, font=f["label"], fill=1)


# Basic: competition's times with no players, as big as they go: a row a stone, newest first, 4 rows,
# B2H black on white left of COMP_MID, H2H white on black right of it, the badge strip below. The
# rocker scrolls (down: older, up: newer), a row a press; scrolled, the badge says which stones show.
BASIC_ROW_H = (H - TIP_H) // 4  # 43
BASIC_ROWS = 4


def basic_rows(stones, waiting):
    """The rows to scroll through, newest first: (b2h, h2h, stopwatch in the B2H cell, in the H2H cell,
    stone number counting from the newest, 1; None for a stone being timed with no time yet)."""
    rows = []
    if waiting in ("b2h", "h2h-only"):  # a stone under way, no time yet: a row of its own
        rows.append((None, None, waiting == "b2h", waiting == "h2h-only", None))
    for i, st in enumerate(stones):
        rows.append((st[0], st[1], False, i == 0 and waiting == "h2h", i + 1))
    return rows


def basic_screen(stones, low_battery=False, tip=False, waiting=None, scroll=0):
    """stones: (b2h, h2h, ...) newest first; scroll: rows scrolled down (to older stones). The top row
    unscrolled is the stone being timed, as the newest row of the other modes."""
    f = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    strip = H - TIP_H
    d.rectangle([COMP_MID, 0, W - 1, strip - 1], fill=0)
    rows = basic_rows(stones, waiting)
    scroll = max(0, min(scroll, len(rows) - BASIC_ROWS))
    shown = rows[scroll:scroll + BASIC_ROWS]
    for i, (bh, hh, watch_bh, watch_hh, _) in enumerate(shown):
        y0 = i * BASIC_ROW_H
        if i:
            d.line([(0, y0), (COMP_MID - 1, y0)], fill=0)  # between rows: black on white, white on black
            d.line([(COMP_MID, y0), (W - 1, y0)], fill=1)
        for k, (x0, x1) in enumerate(((0, COMP_MID), (COMP_MID, W))):
            v, watch = (bh, watch_bh) if k == 0 else (hh, watch_hh)
            if v is not None:
                font = f["big_long"] if v >= 20 else f["big"]
                dseg_number(d, v, font, x0, x1, y0, y0 + BASIC_ROW_H, margin=0, align="center", fill=k)
            elif watch:
                stopwatch(d, (x0 + x1) // 2, y0 + BASIC_ROW_H // 2, STOPWATCH_BIG, k)
    note = None
    if scroll > 0:  # which stones show, counting from the newest
        nums = [n for *_, n in shown if n is not None]
        note = f"{nums[0]}-{nums[-1]} of {len(stones)}"
    badge_strip(img, d, f, "BASIC", low_battery, note)
    if tip in ("undo", "off"):
        tooltip(d, f, UNDO_TIP if tip == "undo" else OFF_TIP, HINT_TOP, strip)
    return img


def competition_screen(stones, me=0, active=0, low_battery=False, tip=False, waiting=None):
    """Times only, with a badge strip an umpire can read at a glance: the four columns as in
    league, 8 lines. stones: (b2h, h2h, player 0-3 or THEM) newest first; me: your position;
    active: the column (0-3) with the cursor, whoever throws next."""
    f = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    strip = H - TIP_H
    four_columns(d, f, stones, me, active, strip, waiting, "zoom_comp")
    badge_strip(img, d, f, "COMPETITION", low_battery)
    if tip in ("undo", "off"):
        tooltip(d, f, UNDO_TIP if tip == "undo" else OFF_TIP, HINT_TOP, strip)
    elif tip == "move":  # over the badge strip, clear of the columns
        tooltip(d, f, MOVE_HINT, strip, H)
    elif tip:
        you_are_tip(d, f, me)
    return img


# League: four columns of times, your three teammates in throwing order and the other team
# (THEM), each listing its stones newest first, a time a line: Back2Hog black on white, Hog2Hog
# white on black. The newest stone is also shown large at the top. Below, your own rocks like a
# scoreboard (ends across, rock 1 over rock 2) with your percentage large on the right.
# The rocker works on one half at a time, the focus, shown by a bold border: the columns (up =
# next column, down = previous: the cursor, the highlighted header, goes on whoever is about to
# throw; it also moves there by itself) or your rocks (up/down = made/missed). Hold down to log your
# rocks; logging one goes back to the columns. Hold up on the columns to move the newest time a
# column right every 0.7 s (as rocker up moves the cursor), put down where it is when let go.
LEAGUE_ROWS_BOTTOM = 159     # 2 px line under the times
LEAGUE_PCT_X = 149           # 2 px divider left of the percentage: in line with the THEM column's
LEAGUE_SLOTS = 16
LEAGUE_X = (2, 198)          # the four columns' outer edges (inside the focus border)
LEAGUE_TOP = 59              # first line
THEM = 4                     # a stone's player: the other team


def league_columns(me):
    """The players in the four columns for you at position me: your teammates, then THEM."""
    return teammates(me) + [THEM]


def league_times(d, f, stones, me, active, bottom, zoom):
    player_columns(d, f, stones, *LEAGUE_X, league_columns(me), active, bottom, f["row"], zoom)


def zoom_span(x0, x1, n, active):
    """The zoomed column's edges: ZOOM_SPREAD columns wide, centred on the cursor's column, kept
    inside x0..x1 (at an edge it takes all the extra from its one neighbour)."""
    zw = round(ZOOM_SPREAD * (x1 - x0) / n)
    za = round(x0 + (x1 - x0) * (active + 0.5) / n - zw / 2)
    za = max(x0, min(x1 - zw, za))
    return za, za + zw


def player_columns(d, f, stones, x0, x1, players, active, bottom, font, zoom):
    """A column per player between x0 and x1, equal widths, headed with the position or THEM. The
    cursor's (active) is zoomed, drawn over its neighbours (zoom_span): its header highlighted, its
    newest 4 times in the biggest digits that fit, filling the column's height. The others list
    theirs newest first, a time a line, in the part the zoom leaves them (narrower digits when it
    leaves too little). B2H black on white, H2H white on black."""
    n = len(players)
    xs = [round(x0 + (x1 - x0) * k / n) for k in range(n + 1)]
    za, zb = zoom_span(x0, x1, n, active)

    def name_of(p):
        return "THEM" if p == THEM else POSITION_NAMES[p]

    def times(k):
        """Column k's times, newest first: (value, H2H?)."""
        return [(v, h) for st in stones if len(st) >= 3 and st[2] == players[k]
                for v, h in ((st[0], False), (st[1], True)) if v is not None]

    def cell(cf, val, inverted, a, b, y, pitch):
        if inverted:
            d.rectangle([a + 1, y, b - 1, y + pitch - 2], fill=0)
        cf.draw_in_box(d, f"{val:.2f}", a, b, y, y + pitch - 1, align="center", fill=1 if inverted else 0)

    most = (bottom - LEAGUE_TOP) // GRID_PITCH
    for k in range(n):
        if k == active:
            continue
        a = max(xs[k], zb) if k > active else xs[k]      # the part the zoom leaves
        b = min(xs[k + 1], za) if k < active else xs[k + 1]
        if k:
            d.line([(a, GRID_LABEL[0]), (a, bottom - 1)], fill=0)
        name = name_of(players[k])
        d.text(((a + b - 6 * len(name)) // 2, GRID_LABEL[0]), name, font=f["label"], fill=0)
        shown = times(k)[:most]
        widest = max((font.width(f"{v:.2f}") for v, _ in shown), default=0)
        cf = font if widest + 4 <= b - a - 2 else f["narrow"]
        for i, (val, inverted) in enumerate(shown):
            cell(cf, val, inverted, a + 1, b - 1, LEAGUE_TOP + i * GRID_PITCH, GRID_PITCH)
    # the zoom: lines at its edges (none at the outer ones), the header highlighted
    if za > x0:
        d.line([(za, GRID_LABEL[0]), (za, bottom - 1)], fill=0)
    if zb < x1:
        d.line([(zb, GRID_LABEL[0]), (zb, bottom - 1)], fill=0)
    d.rectangle([za + (1 if za > x0 else 0), GRID_LABEL[0], zb - 1, GRID_LABEL[1]], fill=0)
    name = name_of(players[active])
    d.text(((za + zb - 6 * len(name)) // 2, GRID_LABEL[0]), name, font=f["label"], fill=1)
    pitch = (bottom - LEAGUE_TOP) // ZOOM_LINES
    for i, (val, inverted) in enumerate(times(active)[:ZOOM_LINES]):
        cell(f[zoom + "_long"] if val >= 20 else f[zoom], val, inverted, za + 1, zb - 1, LEAGUE_TOP + i * pitch, pitch)


def times_top(d, f, stones, waiting, mid, y0=0):
    """The top of the times, every mode: the newest stone large, B2H on white left of mid, H2H on
    black right of it (no labels: as in the columns), a stopwatch where an awaited time will appear.
    y0: the digits' box starts lower (coach: under the page marks)."""
    d.rectangle([mid, 0, W - 1, TIMES_Y1 - 1], fill=0)
    newest_row(d, f, stones, waiting, mid, (f["big"], f["big_long"]), y0, TIMES_Y1, STOPWATCH_BIG,
               (y0 + TIMES_Y1) // 2)
    d.line([(0, TIMES_Y1), (W - 1, TIMES_Y1)], fill=0, width=2)


def four_columns(d, f, stones, me, active, bottom, waiting, zoom):
    """Competition and league times: the newest stone large, then the four columns down to bottom."""
    times_top(d, f, stones, waiting, COMP_MID)
    league_times(d, f, stones, me, active, bottom, zoom)


def league_screen(stones, shots, me=0, active=0, low_battery=False, tip=False, waiting=None, focus="players"):
    """stones: (b2h, h2h, player 0-3 or THEM) newest first; shots: True made / False missed, in
    order; me: your position; active: the column (0-3) with the cursor."""
    f = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    top = LEAGUE_ROWS_BOTTOM
    four_columns(d, f, stones, me, active, top, waiting, "zoom_league")
    d.line([(0, top), (W - 1, top)], fill=0)  # 1 px like the columns: only the focus border is bold
    split = LEAGUE_PCT_X
    d.line([(split, top), (split, H - 1)], fill=0)
    cursor = len(shots) if len(shots) < LEAGUE_SLOTS else -1
    for end in range(8):
        cx = 12 + end * 18  # spread across the box
        f["tiny"].draw(d, str(end + 1), cx - 2, top + 3)
        for r in range(2):
            i = end * 2 + r
            small_rock(d, cx, top + 19 + r * 13, shots[i] if i < len(shots) else None, i == cursor)
    # the focus: a bold border around the half the rocker works on
    if focus == "players":
        d.rectangle([0, TIMES_Y1, W - 1, top + 1], outline=0, width=2)
    else:
        d.rectangle([0, top, W - 1, H - 1], outline=0, width=2)  # the whole bottom: your rocks and your %
    x0, y0 = split + 2, top + 2
    if low_battery:  # a black bar over the percentage
        d.rectangle([x0, y0, W - 3, y0 + 12], fill=0)  # inside the focus border (x 198-199), focused or not
        text_in_box(d, "Low bat", f["label"], x0, W - 2, y0, y0 + 12, align="center", fill=1)
        y0 += 13
    if shots:
        big_percent(d, f["pct"], percent_of(sum(shots), len(shots)), x0, W - 2, y0, H)
    if tip in ("undo", "off"):
        tooltip(d, f, UNDO_TIP if tip == "undo" else OFF_TIP, HINT_TOP, top)
    elif tip == "move":  # over your rocks, clear of the columns
        tooltip(d, f, MOVE_HINT, top, H)
    elif tip:  # below the headers, over the rocks too: it doesn't fit above them
        you_are_tip(d, f, me)
    return img


def small_rock(d, cx, cy, state, cursor):
    """A rock (radius 5): an empty ring not yet thrown, filled made, a dot in the ring missed; bold
    ring = cursor."""
    d.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], outline=0, width=1, fill=0 if state is True else None)
    if state is False:
        d.ellipse([cx - 2, cy - 2, cx + 2, cy + 2], fill=0)
    if cursor:
        d.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], outline=0, width=3)


def page_marks(d, page):
    """Two small squares at the top right, white on the H2H half's black: filled = the page showing."""
    for k in range(2):
        x = W - 12 + k * 6
        if k == page:
            d.rectangle([x, 1, x + 3, 4], fill=1)
        else:
            d.rectangle([x, 1, x + 3, 4], outline=1)


def position_percent(shots, p):
    """Rounded % made for position p (0 lead .. 3 skip), or None before its first rock."""
    xs = [m for i, m in enumerate(shots) if (i % 8) // 2 == p]
    return percent_of(sum(xs), len(xs)) if xs else None


def coach_rock(d, i, shots, cx, cy):
    state = shots[i] if i < len(shots) else None
    small_rock(d, cx, cy, state, i == len(shots))


COACH_TIMES_Y0 = 4  # coach: the newest stone's digits start under the page marks (y 1-4)


def coach_timer_screen(stones, shots, waiting=None, active=0, tip=False, focus="players"):
    """Coach page 1, timer priority: the newest stone large, then a column per position and THEM
    (active: the column, 0-4, with the cursor), the previous and current end below.
    stones: (b2h, h2h, player 0-3 or THEM) newest first."""
    f = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    bottom = 150
    times_top(d, f, stones, waiting, WIDE_MID, COACH_TIMES_Y0)  # the digits under the page marks
    page_marks(d, 0)
    player_columns(d, f, stones, 0, W, [0, 1, 2, 3, THEM], active, bottom, f["narrow"], "zoom_coach")
    d.line([(0, bottom), (W - 1, bottom)], fill=0, width=2)
    gx = COACH_GX
    for p in range(4):
        if p:
            d.line([(gx[p], bottom), (gx[p], H - 1)], fill=0)
        v = position_percent(shots, p)
        label = f"{POSITION_SHORT[p]} {v}%" if v is not None else POSITION_SHORT[p]
        text_in_box(d, label, f["label"], gx[p], gx[p + 1], bottom + 2, bottom + 14, align="center")
    current = min(len(shots), COACH_SLOTS - 1) // 8
    first = max(0, min(current - 1, 6))
    for row, end in enumerate((first, first + 1)):
        cy = bottom + 24 + row * 15
        f["tiny"].draw(d, str(end + 1), 3, cy - 4)
        for p in range(4):
            for r in range(2):
                coach_rock(d, end * 8 + p * 2 + r, shots, gx[p] + 15 + r * 17, cy)
    # the focus, as in league: a bold border round the times or the rocks
    if focus == "players":
        d.rectangle([0, TIMES_Y1, W - 1, bottom + 1], outline=0, width=2)
    else:
        d.rectangle([0, bottom, W - 1, H - 1], outline=0, width=2)
    if tip in ("undo", "off"):
        tooltip(d, f, UNDO_TIP if tip == "undo" else OFF_TIP, HINT_TOP, bottom)
    elif tip == "page":
        tooltip(d, f, PAGE_TIP, HINT_TOP, bottom)
    elif tip == "move":  # over the rocks, clear of the columns
        tooltip(d, f, MOVE_HINT, bottom, H)
    return img


def coach_rocks_screen(stones, shots, waiting=None, tip=False, focus="players"):
    """Coach page 2, rock priority: newest times only, all 64 rocks, % per position."""
    f = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    mid = WIDE_MID
    d.rectangle([mid, 0, W - 1, 32], fill=0)  # H2H white on black, as on page 1
    newest_row(d, f, stones, waiting, mid, (f["top_small"], f["top_small"]), COACH_TIMES_Y0, 33,
               STOPWATCH_SMALL, (COACH_TIMES_Y0 + 33) // 2)
    page_marks(d, 1)
    d.line([(0, 33), (W - 1, 33)], fill=0, width=2)
    gx = COACH_GX
    for p in range(4):
        text_in_box(d, POSITIONS[p], f["label"], gx[p], gx[p + 1], 35, 47, align="center")
        if p:
            d.line([(gx[p], 35), (gx[p], 185)], fill=0)
    d.line([(0, 47), (W - 1, 47)], fill=0)
    for end in range(8):
        cy = 55 + end * 16
        f["tiny"].draw(d, str(end + 1), 3, cy - 4)
        for p in range(4):
            for r in range(2):
                coach_rock(d, end * 8 + p * 2 + r, shots, gx[p] + 15 + r * 17, cy)
    d.line([(0, 185), (W - 1, 185)], fill=0, width=2)
    for p in range(4):
        if p:
            d.line([(gx[p], 185), (gx[p], H - 1)], fill=0)
        v = position_percent(shots, p)
        if v is not None:
            txt = f"{v}%"
            w = 13 + 6 * len(txt)
            x = (gx[p] + gx[p + 1] - w) // 2
            draw_pie(d, x + 5, 193, v)
            d.text((x + 13, 187), txt, font=f["label"], fill=0)
    if focus == "players":
        d.rectangle([0, 0, W - 1, 34], outline=0, width=2)
    else:
        d.rectangle([0, 33, W - 1, 186], outline=0, width=2)
    if tip in ("undo", "off"):
        tooltip(d, f, UNDO_TIP if tip == "undo" else OFF_TIP, 47, 185)  # over the rocks
    elif tip == "page":
        tooltip(d, f, PAGE_TIP, 47, 185)
    elif tip == "move":
        tooltip(d, f, MOVE_HINT, 150, H)
    return img


def to_rgb(img, z):
    """Enlarge a 1-bit screen and tint it like e-ink."""
    big = img.resize((img.width * z, img.height * z), Image.NEAREST)
    out = Image.new("RGB", big.size, (222, 222, 214))
    out.paste(Image.new("RGB", big.size, (25, 25, 25)), (0, 0),
              big.convert("L").point(lambda v: 255 if v < 128 else 0))
    return out
