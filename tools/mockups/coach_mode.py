"""Mockup of the planned pro/coach mode (docs/PRO_MODE.md): two pages, timer priority and rock priority.

    python tools/mockups/coach_mode.py
"""
from pathlib import Path
import random
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from PIL import Image, ImageDraw, ImageFont  # noqa: E402

import screen as s  # noqa: E402

OUT = Path(__file__).resolve().parents[2] / "docs" / "mockups" / "pro" / "coach-mode.png"
W = H = 200
POS = ["Lead", "2nd", "3rd", "Skip"]
SHORT = ["L", "2", "3", "S"]

# sample game: ends 1-4 done, end 5 under way (lead's two rocks and second's first logged)
random.seed(7)
odds = [0.8, 0.7, 0.65, 0.6]
rocks = []  # (end, position, rock 0/1, made)
for end in range(8):
    for p in range(4):
        for r in range(2):
            rocks.append((end, p, r, random.random() < odds[p]))
LOGGED = 4 * 8 + 3
CURSOR = LOGGED  # next rock: end 5, second, rock 2

STONES = [(3.47, 13.92), (3.29, None), (3.51, 13.66), (None, 14.02), (3.38, None), (3.42, 13.81),
          (3.61, 14.20), (3.33, 13.75), (None, 13.88), (3.40, 14.05), (3.55, None), (3.36, 13.97)]


def pct(p):
    xs = [m for i, (e, q, r, m) in enumerate(rocks[:LOGGED]) if q == p]
    return round(100 * sum(xs) / len(xs)) if xs else None


def rock(d, cx, cy, i, r=5):
    """Rock slot i at (cx, cy): empty circle, check (made), X (missed), bold ring = cursor."""
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=0, width=1)
    if i < LOGGED:
        if rocks[i][3]:
            d.line([(cx - 3, cy), (cx - 1, cy + 2), (cx + 3, cy - 3)], fill=0, width=2)
        else:
            d.line([(cx - 2, cy - 2), (cx + 2, cy + 2)], fill=0, width=2)
            d.line([(cx - 2, cy + 2), (cx + 2, cy - 2)], fill=0, width=2)
    if i == CURSOR:
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=0, width=3)


def page_dots(d, which):
    for k in range(2):
        x = W - 16 + k * 8
        if k == which:
            d.ellipse([x, 3, x + 5, 8], fill=0)
        else:
            d.ellipse([x, 3, x + 5, 8], outline=0)


def times_header(d, f, x_split):
    s.text_in_box(d, "Back2Hog", f["label"], 0, x_split, 0, 12, align="center")
    s.text_in_box(d, "Hog2Hog", f["label"], x_split, W - 18, 0, 12, align="center")
    d.line([(0, 12), (W - 1, 12)], fill=0)


def small_pct(d, f, value, x0, x1, y):
    """Pie + percentage, centred in [x0, x1]."""
    txt = f"{value}%"
    w = 13 + 6 * len(txt)
    x = (x0 + x1 - w) // 2
    s.draw_pie(d, x + 5, y + 6, value)
    d.text((x + 13, y), txt, font=f["label"], fill=0)


def timer_page():
    """Timer priority: wide times on top, the last two ends' 16 rocks along the bottom."""
    f = s.fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    mid = 100
    times_header(d, f, mid)
    page_dots(d, 0)
    # newest, large
    b, h = STONES[0]
    s.dseg_number(d, b, f["big"], 0, mid, 13, 47)
    s.dseg_number(d, h, f["big"], mid, W, 13, 47)
    d.line([(0, 47), (W - 1, 47)], fill=0, width=2)
    bottom_top = 150
    d.line([(mid, 0), (mid, bottom_top - 1)], fill=0)
    # older results, as many rows as fit
    for i, (bh, hh) in enumerate(STONES[1:]):
        ry = 50 + i * 14
        if ry + 14 > bottom_top:
            break
        for val, x0, x1 in ((bh, 0, mid), (hh, mid, W)):
            if val is not None:
                s.text_in_box(d, f"{val:.2f}", f["row"], x0, x1, ry, ry + 14, margin=14)
    # rocks: four columns (lead, second, third, skip), previous and current end
    d.line([(0, bottom_top), (W - 1, bottom_top)], fill=0, width=2)
    gx = [12 + k * 47 for k in range(5)]
    for p in range(4):
        x0, x1 = gx[p], gx[p + 1]
        if p:
            d.line([(x0, bottom_top), (x0, H - 1)], fill=0)
        v = pct(p)
        label = f"{SHORT[p]} {v}%" if v is not None else SHORT[p]
        s.text_in_box(d, label, f["label"], x0, x1, bottom_top + 2, bottom_top + 14, align="center")
    cur_end = CURSOR // 8
    for row, end in enumerate((cur_end - 1, cur_end)):
        cy = bottom_top + 24 + row * 15
        f["tiny"].draw(d, str(end + 1), 3, cy - 4)
        for p in range(4):
            for r in range(2):
                i = end * 8 + p * 2 + r
                cx = gx[p] + 15 + r * 17
                rock(d, cx, cy, i)
    return img


def rocks_page():
    """Rock priority: only the newest times on top; all 64 rocks; running % per position."""
    f = s.fonts()
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"
    mid = 100
    times_header(d, f, mid)
    page_dots(d, 1)
    b, h = STONES[0]
    s.dseg_number(d, b, f["pct"], 0, mid, 13, 33, margin=20)
    s.dseg_number(d, h, f["pct"], mid, W, 13, 33, margin=20)
    d.line([(mid, 0), (mid, 33)], fill=0)
    d.line([(0, 33), (W - 1, 33)], fill=0, width=2)
    # position headers
    gx = [12 + k * 47 for k in range(5)]
    for p in range(4):
        s.text_in_box(d, POS[p], f["label"], gx[p], gx[p + 1], 35, 47, align="center")
        if p:
            d.line([(gx[p], 35), (gx[p], 185)], fill=0)
    d.line([(0, 47), (W - 1, 47)], fill=0)
    # 8 ends x 8 rocks
    for end in range(8):
        cy = 55 + end * 16
        f["tiny"].draw(d, str(end + 1), 3, cy - 4)
        for p in range(4):
            for r in range(2):
                i = end * 8 + p * 2 + r
                rock(d, gx[p] + 15 + r * 17, cy, i)
    # running percentage per position
    d.line([(0, 185), (W - 1, 185)], fill=0, width=2)
    for p in range(4):
        v = pct(p)
        if v is not None:
            small_pct(d, f, v, gx[p], gx[p + 1], 187)
        if p:
            d.line([(gx[p], 185), (gx[p], H - 1)], fill=0)
    return img


def main():
    z, pad, gap, bezel = 3, 36, 40, 14
    cell = W * z + 2 * bezel
    cw = pad * 2 + 2 * cell + gap
    ch = pad + 50 + 70 + cell + 150
    canvas = Image.new("RGB", (cw, ch), (246, 246, 244))
    d = ImageDraw.Draw(canvas)
    bold = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24)
    tf = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 18)
    nf = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 15)
    d.text((pad, pad - 8), "Skip / coach mode: two pages (double-press the rocker to switch)", font=bold, fill=(30, 30, 30))
    pages = [
        ("Page 1: timer priority", "Wide times on top; the last two ends' 16 rocks below, by position", timer_page()),
        ("Page 2: rock priority", "Newest times only; all 64 rocks; running % per position", rocks_page()),
    ]
    for i, (title, note, img) in enumerate(pages):
        x = pad + i * (cell + gap)
        y = pad + 50
        d.text((x, y), title, font=tf, fill=(30, 30, 30))
        d.text((x, y + 26), note, font=nf, fill=(100, 100, 100))
        by = y + 60
        d.rounded_rectangle([x, by, x + cell, by + cell], radius=16, fill=(205, 207, 210), outline=(150, 150, 150), width=2)
        canvas.paste(s.to_rgb(img, z), (x + bezel, by + bezel))
    notes = [
        "Sample: end 5, lead's two rocks and second's first logged; bold ring = next rock (second, rock 2).",
        "The rocker logs made/missed for each team rock in throwing order: lead, lead, second, second, third, third, skip, skip.",
        "Dots top right: which page. Percentages are per position, over rocks logged so far.",
    ]
    for i, t in enumerate(notes):
        d.text((pad, pad + 50 + 70 + cell + 20 + i * 24), t, font=nf, fill=(70, 70, 70))
    canvas.save(OUT)
    print("wrote", OUT.relative_to(OUT.parents[3]))


if __name__ == "__main__":
    main()
