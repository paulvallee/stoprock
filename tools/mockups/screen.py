"""Pixel-exact mockups of the stoprock screens (200 x 200, 1-bit), as agreed in docs/SPEC.md."""
from pathlib import Path
import tempfile

from PIL import BdfFontFile, Image, ImageChops, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
FONTS = ROOT / "assets" / "fonts"
DSEG = FONTS / "DSEG7Classic-Bold.ttf"

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
    glyphs(d, ip, font, round(x - li), round(y))
    xd = x + (ri - li) + gap
    yb = y + b
    d.rectangle([round(xd), round(yb - dot), round(xd + dot - 1), round(yb - 1)], fill=0)
    glyphs(d, fp, font, round(xd + dot + gap - lf), round(y))


def glyphs(d, s, font, x, y):
    """Draw s one glyph at a time, each at a whole-pixel advance: as the firmware does.
    (Pillow lays a string out at fractional advances, which can shift a glyph by a pixel.)"""
    for ch in s:
        d.text((x, y), ch, font=font, fill=0)
        x += round(font.getlength(ch))


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

    def _digit(self, d, ch, x, y, fill=0):
        w, h, t = self.w, self.h, self.t
        segs = SEGMENTS.get(ch, "")
        m = (h - t) // 2
        c = 1 if t == 1 else t - 1
        extra = 1 if t == 1 else 0

        def r(x0, y0, x1, y1):
            d.rectangle([x + x0, y + y0, x + x1 - 1, y + y1 - 1], fill=fill)

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
    return dict(
        label=bdf("spleen-6x12"),
        big=ImageFont.truetype(str(DSEG), 25),
        row=SegFont(h=12, w=7, t=2, gap=2, dot=2),
        tiny=SegFont(h=9, w=5, t=1, gap=1),  # equal-width cells: a "1" sits on the right
        narrow=SegFont(h=12, w=6, t=2, gap=1, dot=2, prop=True),  # coach's five columns: a narrow "1"
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


# --------------------------------------------------------------------------- help page
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


HELP_EQ_X = 5 + 6 * len("SHOTS") + 8 + 6 * len("up/down ")  # x of every "=" on the help page


def help_page(device_logo, last_game, shot_pct=None, battery=None, battery_note=None):
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

    # every "=" on the page sits in one column, where SHOTS' "up/down = " puts it
    eq_x = HELP_EQ_X

    def eq_line(txt):
        nonlocal y
        d.text((eq_x - 6 * txt.index("="), y), txt, font=f, fill=0)
        y += 12

    tag("START", "hold side button, plus:")
    eq_line("nothing     = competition mode")
    eq_line("rocker down = league mode")
    eq_line("rocker up   = coach mode")
    y += 5
    tag("TIMING", "use top or side button")
    eq_line("hold up = move last time")
    y += 3
    tag("SHOTS", "up/down = made/missed")
    y += 3
    tag("UNDO", "hold centre: time or shot")
    y += 4
    d.line([(4, y), (195, y)], fill=0)
    y += 5
    # Bottom right: shot percentage with a pie, battery with its icon. The icons end at the
    # same x (room for "100%" after them), and the percentages are right-aligned so the
    # % signs line up. That leaves 26 characters for "Last game 16/16 shots made".
    icon_right, pct_right = W - 29, W - 2

    def percent(value):
        pct = f"{min(100, max(0, value))}%"
        d.text((pct_right - 6 * len(pct), y), pct, font=f, fill=0)

    d.text((4, y), last_game, font=f, fill=0)
    if shot_pct is not None:
        draw_pie(d, icon_right - 5, y + 6, shot_pct)
        percent(shot_pct)
    y += 12
    if battery_note:  # e.g. "~11 games battery left", from past games' drain
        d.text((4, y), battery_note, font=f, fill=0)
    if battery is not None:
        bx = icon_right - 14
        d.rectangle([bx, y + 3, bx + 12, y + 9], outline=0)
        d.rectangle([bx + 13, y + 5, bx + 14, y + 7], fill=0)
        fill = round(9 * max(0, min(100, battery)) / 100)
        if fill > 0:
            d.rectangle([bx + 2, y + 5, bx + 2 + fill - 1, y + 7], fill=0)
        percent(battery)
        y += 12
    assert y <= H, f"help page overflows: {y}"
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
COMP_MID = 92
GRID_LABEL = (49, 60)        # player headers
GRID_PITCH = 14
POSITION_NAMES = ("Lead", "2nd", "3rd", "Skip")


def teammates(me):
    """The three positions timed by a player at position me (0 lead .. 3 skip), in throwing order."""
    return [p for p in range(4) if p != me]


def stopwatch(d, cx, cy, r):
    """A stopwatch: a 2 px ring of radius r, the crown on top, one hand at about 1 o'clock."""
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=0, width=2)
    d.rectangle([cx - 1, cy - r - 2, cx + 1, cy - r - 1], fill=0)   # stem
    d.rectangle([cx - 3, cy - r - 4, cx + 3, cy - r - 3], fill=0)   # button
    d.line([(cx, cy), (cx + r // 2, cy - r // 2 - 1)], fill=0, width=2)
    d.rectangle([cx - 1, cy - 1, cx + 1, cy + 1], fill=0)           # centre


STOPWATCH_BIG, STOPWATCH_SMALL = 10, 6  # ring radius: in the large row, and coach page 2's


def newest_row(d, f, stones, waiting, mid, font, y0, y1, margin, r, cy):
    """The current stone across both halves. waiting: "b2h", "h2h" or "h2h-only" while it's being
    timed (a stopwatch where that time will appear; for "b2h" and "h2h-only", a stone whose first
    click was 5.5 s ago with no second, so it can only be an H2H, the row is still empty)."""
    cur = None if waiting in ("b2h", "h2h-only") or not stones else stones[0]
    for k, (x0, x1) in enumerate(((0, mid), (mid, W))):
        if cur is not None and cur[k] is not None:
            dseg_number(d, cur[k], font, x0, x1, y0, y1, margin=margin)
        elif waiting == ("b2h", "h2h")[k] or (k == 1 and waiting == "h2h-only"):
            stopwatch(d, (x0 + x1) // 2, cy, r)


# Switch-on tips ("" = a half-line gap): your position, how to change it, and in league (not
# competition, which has no holds) what holding the rocker does. Also shown when you change position.
YOU_ARE = ("LEAD", "SECOND", "THIRD", "SKIP")
POSITION_TIP = ("Double click rocker to", "change your position")
MOVE_TIP = "Hold rocker up: move last time"
UNDO_LINE = "Hold rocker middle: undo"  # the switch-on tips' last line
UNDO_TIP = ("Keep holding to undo",)
OFF_TIP = ("Keep holding to turn off",)  # tip="off": the side button held, before it switches off (3 s)
MOVE_HINT = ("Keep holding to move time",)  # tip="move": rocker held to move the newest time, over the bottom  # tip="undo": while the rocker centre is held, before the undo (1 s)
LEAGUE_TIP = ("Hold rocker down: your rocks", MOVE_TIP, UNDO_LINE)


def tooltip(d, f, lines, top, bottom, headers=True):
    """A bordered box centred in top..bottom (the times area, or the bottom of the screen with
    headers=False), lines centred; an empty line is a half-line gap."""
    w = 6 * max(len(t) for t in lines) + 16
    h = sum(12 if t else 6 for t in lines) + 14
    x0, y0 = (W - w) // 2, top + (bottom - top - h) // 2
    if headers and y0 < top + 12:  # it would cut through the column headers: cover them instead
        y0 = top
    y1 = y0 + h - 1
    d.rectangle([x0, y0, x0 + w - 1, y1], fill=1, outline=0, width=2)
    if headers and y0 == top:  # against the 2 px lines above and beside it: a bottom as bold
        d.rectangle([x0, y1 + 1, x0 + w - 1, y1 + 2], fill=0)
    y = y0 + 7
    for t in lines:
        if t:
            d.text(((W - 6 * len(t)) // 2, y), t, font=f["label"], fill=0)
        y += 12 if t else 6


def competition_screen(stones, me=0, active=0, low_battery=False, tip=False, waiting=None):
    """Times only, with a badge strip an umpire can read at a glance: the four columns as in
    league, 8 lines. stones: (b2h, h2h, player 0-3 or THEM) newest first; me: your position;
    active: the column (0-3) with the cursor, whoever throws next."""
    f = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    strip = H - TIP_H
    four_columns(d, f, stones, me, active, strip, 8, waiting)
    # the badge strip: the logo white on black, and two lines right-aligned
    d.rectangle([0, strip, W - 1, H - 1], fill=0)
    from logo import strip_logo  # here, not at the top: logo.py is a sibling module
    lg = strip_logo()
    img.paste(1, (2, strip + (TIP_H - lg.height) // 2), ImageChops.invert(lg.convert("L")).convert("1"))
    sub = "Battery low" if low_battery else "STOPWATCH ONLY"
    for t, y in (("COMPETITION", strip + 1), (sub, strip + 13)):
        d.text((W - 2 - 6 * len(t), y), t, font=f["label"], fill=1)
    if tip in ("undo", "off"):
        tooltip(d, f, UNDO_TIP if tip == "undo" else OFF_TIP, 49, strip)
    elif tip == "move":  # over the badge strip, clear of the columns
        tooltip(d, f, MOVE_HINT, strip, H, headers=False)
    elif tip:
        tooltip(d, f, (f"You are {YOU_ARE[me]}", "") + POSITION_TIP + ("", MOVE_TIP, UNDO_LINE), 49, strip)
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
LEAGUE_COLS = [2, 51, 100, 149, 198]  # edges of the four columns
LEAGUE_TOP = 62              # first line
LEAGUE_LINES = 7
THEM = 4                     # a stone's player: the other team


def league_columns(me):
    """The players in the four columns for you at position me: your teammates, then THEM."""
    return teammates(me) + [THEM]


def league_times(d, f, stones, me, active, bottom, lines_max=LEAGUE_LINES):
    player_columns(d, f, stones, LEAGUE_COLS, league_columns(me), active, bottom, lines_max, f["row"])


def player_columns(d, f, stones, xs, players, active, bottom, lines_max, font):
    """A column per player (edges xs), headed with the position or THEM, the active one
    highlighted; each lists its stones newest first, a time a line, H2H white on black."""
    for k, p in enumerate(players):
        x0, x1 = xs[k], xs[k + 1]
        if k:
            d.line([(x0, GRID_LABEL[0]), (x0, bottom - 1)], fill=0)
        on = k == active
        if on:
            d.rectangle([x0 + (1 if k else 0), GRID_LABEL[0], x1 - 1, GRID_LABEL[1]], fill=0)
        name = "THEM" if p == THEM else POSITION_NAMES[p]
        d.text(((x0 + x1 - 6 * len(name)) // 2, GRID_LABEL[0]), name, font=f["label"], fill=1 if on else 0)
    lines = [0] * len(players)
    for stone in stones:
        if len(stone) < 3 or stone[2] not in players:
            continue
        k = players.index(stone[2])
        x0, x1 = xs[k] + 1, xs[k + 1] - 1
        for val, inverted in ((stone[0], False), (stone[1], True)):
            if val is None or lines[k] >= lines_max:
                continue
            y = LEAGUE_TOP + lines[k] * GRID_PITCH
            lines[k] += 1
            if inverted:
                d.rectangle([x0 + 1, y, x1 - 1, y + GRID_PITCH - 2], fill=0)
            font.draw_in_box(d, f"{val:.2f}", x0, x1, y, y + GRID_PITCH - 1, align="center",
                             fill=1 if inverted else 0)


def times_top(d, f, stones, waiting, mid, label_right=W):
    """The top of the times, every mode: Back2Hog | Hog2Hog labels (split at mid; the H2H label ends
    at label_right), the newest stone large, a stopwatch where an awaited time will appear."""
    text_in_box(d, "Back2Hog", f["label"], 0, mid, 0, 12, align="center")
    text_in_box(d, "Hog2Hog", f["label"], mid, label_right, 0, 12, align="center")
    d.line([(0, 12), (W - 1, 12)], fill=0)
    newest_row(d, f, stones, waiting, mid, f["big"], 13, 47, 3, STOPWATCH_BIG, 30)
    d.line([(0, 47), (W - 1, 47)], fill=0, width=2)
    d.line([(mid, 0), (mid, 46)], fill=0)


def four_columns(d, f, stones, me, active, bottom, lines_max, waiting):
    """Competition and league times: the newest stone large, then the four columns down to bottom."""
    times_top(d, f, stones, waiting, COMP_MID)
    league_times(d, f, stones, me, active, bottom, lines_max)


def league_screen(stones, shots, me=0, active=0, low_battery=False, tip=False, waiting=None, focus="players"):
    """stones: (b2h, h2h, player 0-3 or THEM) newest first; shots: True made / False missed, in
    order; me: your position; active: the column (0-3) with the cursor."""
    f = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    top = LEAGUE_ROWS_BOTTOM
    four_columns(d, f, stones, me, active, top, LEAGUE_LINES, waiting)
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
        d.rectangle([0, 47, W - 1, top + 1], outline=0, width=2)
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
        tooltip(d, f, UNDO_TIP if tip == "undo" else OFF_TIP, 49, top)
    elif tip == "move":  # over your rocks, clear of the columns
        tooltip(d, f, MOVE_HINT, top, H, headers=False)
    elif tip:
        tooltip(d, f, (f"You are {YOU_ARE[me]}", "") + POSITION_TIP + ("",) + LEAGUE_TIP, 49, top)
    return img


def small_rock(d, cx, cy, state, cursor):
    """Coach-mode rock (radius 5): empty, made (check) or missed (X); bold ring = cursor."""
    d.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], outline=0, width=1)
    if state is True:
        d.line([(cx - 3, cy), (cx - 1, cy + 2), (cx + 3, cy - 3)], fill=0, width=2)
    elif state is False:
        d.line([(cx - 2, cy - 2), (cx + 2, cy + 2)], fill=0, width=2)
        d.line([(cx - 2, cy + 2), (cx + 2, cy - 2)], fill=0, width=2)
    if cursor:
        d.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], outline=0, width=3)


def page_marks(d, page):
    """Two small squares top right: filled = the page showing."""
    for k in range(2):
        x = W - 16 + k * 8
        if k == page:
            d.rectangle([x, 3, x + 5, 8], fill=0)
        else:
            d.rectangle([x, 3, x + 5, 8], outline=0)


def position_percent(shots, p):
    """Rounded % made for position p (0 lead .. 3 skip), or None before its first rock."""
    xs = [m for i, m in enumerate(shots) if (i % 8) // 2 == p]
    return percent_of(sum(xs), len(xs)) if xs else None


def coach_rock(d, i, shots, cx, cy):
    state = shots[i] if i < len(shots) else None
    small_rock(d, cx, cy, state, i == len(shots))


COACH_COLS = [0, 40, 80, 120, 160, 200]  # page 1's five columns: Lead, 2nd, 3rd, Skip, THEM
COACH_LINES = 6
COACH_TIP = ("Double click rocker to", "switch page", "", "Hold rocker down: rocks", MOVE_TIP, UNDO_LINE)


def coach_timer_screen(stones, shots, waiting=None, active=0, tip=False, focus="players"):
    """Coach page 1, timer priority: the newest stone large, then a column per position and THEM
    (active: the column, 0-4, with the cursor), the previous and current end below.
    stones: (b2h, h2h, player 0-3 or THEM) newest first."""
    f = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    page_marks(d, 0)
    bottom = 150
    times_top(d, f, stones, waiting, WIDE_MID, W - 18)  # left of the page marks
    player_columns(d, f, stones, COACH_COLS, [0, 1, 2, 3, THEM], active, bottom, COACH_LINES, f["narrow"])
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
        d.rectangle([0, 47, W - 1, bottom + 1], outline=0, width=2)
    else:
        d.rectangle([0, bottom, W - 1, H - 1], outline=0, width=2)
    if tip in ("undo", "off"):
        tooltip(d, f, UNDO_TIP if tip == "undo" else OFF_TIP, 49, bottom)
    elif tip == "move":  # over the rocks, clear of the columns
        tooltip(d, f, MOVE_HINT, bottom, H, headers=False)
    elif tip:
        tooltip(d, f, COACH_TIP, 49, bottom)
    return img


def coach_rocks_screen(stones, shots, waiting=None, tip=False, focus="players"):
    """Coach page 2, rock priority: newest times only, all 64 rocks, % per position."""
    f = fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    mid = WIDE_MID
    text_in_box(d, "Back2Hog", f["label"], 0, mid, 0, 12, align="center")
    text_in_box(d, "Hog2Hog", f["label"], mid, W - 18, 0, 12, align="center")
    d.line([(0, 12), (W - 1, 12)], fill=0)
    page_marks(d, 1)
    newest_row(d, f, stones, waiting, mid, f["pct"], 13, 33, 20, STOPWATCH_SMALL, 25)
    d.line([(mid, 0), (mid, 33)], fill=0)
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
        d.rectangle([0, 12, W - 1, 34], outline=0, width=2)
    else:
        d.rectangle([0, 33, W - 1, 186], outline=0, width=2)
    if tip in ("undo", "off"):
        tooltip(d, f, UNDO_TIP if tip == "undo" else OFF_TIP, 47, 185)  # over the rocks
    elif tip == "move":
        tooltip(d, f, MOVE_HINT, 150, H, headers=False)
    return img


def to_rgb(img, z):
    """Enlarge a 1-bit screen and tint it like e-ink."""
    big = img.resize((img.width * z, img.height * z), Image.NEAREST)
    out = Image.new("RGB", big.size, (222, 222, 214))
    out.paste(Image.new("RGB", big.size, (25, 25, 25)), (0, 0),
              big.convert("L").point(lambda v: 255 if v < 128 else 0))
    return out
