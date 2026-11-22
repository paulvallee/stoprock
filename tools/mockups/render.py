"""Regenerate the mockups in docs/mockups and the logo files in assets/logo.

    python tools/mockups/render.py
"""
from pathlib import Path
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import logo  # noqa: E402
import screen  # noqa: E402

ROOT = screen.ROOT
DOCS = ROOT / "docs" / "mockups"
LOGO = ROOT / "assets" / "logo"

TIPS = [
    ("Rocker up: made shot", "Rocker down: missed"),
    ("Hold rocker to undo", "keep holding to undo more"),
    ("Back2Hog: back/tee to hog", "click far hog for Hog2Hog"),
    ("Top or side button both", "time. Use either one."),
    ("Two clicks within 1 s", "count as one"),
    ("After an H2H, the next", "click starts a new stone"),
    ("Each switch-on starts", "a new game"),
    ("Off after 5 min idle.", "Hold side to switch on"),
]
WARNING = ("Battery low", "Charge after this game")

STONES = [(3.47, 13.92), (3.29, None), (3.51, 13.66), (None, 14.02), (3.38, None), (3.42, 13.81)]
BASE = [True, True, False, True, True, True]  # ends 1-3
# coach mode sample: end 5 under way (lead's two rocks and second's first logged)
COACH_SHOTS = [i % 5 != 3 and i % 7 != 5 for i in range(35)]
COACH_STONES = [(3.47, 13.92), (3.29, None), (3.51, 13.66), (None, 14.02), (3.38, None), (3.42, 13.81),
                (3.61, 14.20), (3.33, 13.75), (None, 13.88), (3.40, 14.05), (3.55, None), (3.36, 13.97)]

# competition sample: you're lead; 25 of your teammates' stones newest first, two each in turn
# (2nd, 2nd, 3rd, 3rd, Skip, Skip, ...), and the third's first rock of this end just timed.
COMP_ME = 0
COMP_STONES = [(None if i % 5 == 3 else (320 + i * 37 % 60) / 100,
                None if i % 7 == 4 else (1350 + i * 53 % 90) / 100,
                1 + (i + 3) // 2 % 3) for i in range(25)]
COMP_ACTIVE = 1


def caption_font(size, bold=False):
    path = f"/usr/share/fonts/truetype/dejavu/DejaVuSans{'-Bold' if bold else ''}.ttf"
    try:
        return ImageFont.truetype(path, size)
    except OSError:
        return ImageFont.load_default()


def sheet(title, panels, out, cols=3, z=2, header=None):
    """panels: (caption, note, 1-bit screen). Laid out in a grid with device bezels."""
    pad, gap, bezel = 36, 36, 14
    cell = 200 * z + 2 * bezel
    rows = (len(panels) + cols - 1) // cols
    head_h = header.height + 30 if header else 0
    cw = pad * 2 + cols * cell + (cols - 1) * gap
    ch = pad + 44 + head_h + rows * (60 + cell) + (rows - 1) * gap + pad
    canvas = Image.new("RGB", (cw, ch), (246, 246, 244))
    d = ImageDraw.Draw(canvas)
    d.text((pad, pad - 8), title, font=caption_font(22, True), fill=(30, 30, 30))
    y0 = pad + 44
    if header:
        canvas.paste(header, ((cw - header.width) // 2, y0))
        y0 += head_h
    for i, (cap, note, img) in enumerate(panels):
        x = pad + (i % cols) * (cell + gap)
        y = y0 + (i // cols) * (60 + cell + gap)
        d.text((x, y), cap, font=caption_font(17, True), fill=(30, 30, 30))
        d.text((x, y + 24), note, font=caption_font(13), fill=(100, 100, 100))
        by = y + 56
        d.rounded_rectangle([x, by, x + cell, by + cell], radius=14, fill=(205, 207, 210), outline=(150, 150, 150), width=2)
        canvas.paste(screen.to_rgb(img, z), (x + bezel, by + bezel))
    canvas.save(out)
    print("wrote", out.relative_to(ROOT))


def main():
    for a, b in TIPS + [WARNING]:
        assert len(a) <= 25 and len(b) <= 25, (a, b)
    DOCS.mkdir(parents=True, exist_ok=True)
    LOGO.mkdir(parents=True, exist_ok=True)

    wm = logo.wordmark()
    wm.save(LOGO / "stoprock-logo.png")
    dev = logo.device_logo()
    dev.save(LOGO / "stoprock-logo-176px-1bit.png")
    print("wrote", (LOGO / "stoprock-logo.png").relative_to(ROOT))

    header = wm.resize((620, round(wm.height * 620 / wm.width)), Image.LANCZOS).convert("RGB")
    sheet("stoprock screens", [
        ("Switched off: help page", "Shown after power-off; e-ink keeps it with no power",
         screen.help_page(dev, "Last game 9/12 shots made", shot_pct=75, battery=84, battery_note="~11 games battery left")),
        ("Main screen with a tip", "Tip 4 of 8; the next tip shows after each timed stone",
         screen.main_screen(STONES, BASE + [False], tip=TIPS[3])),
        ("Low battery", "Warning replaces the tip until charged",
         screen.main_screen(STONES, BASE + [False], tip=WARNING, warn=True)),
    ], DOCS / "screens.png", header=header)

    sheet("Start-up modes: hold the side button to switch on, plus the rocker to choose", [
        ("Off page", "How to start each mode; last game and battery",
         screen.help_page(dev, "Last game 9/12 shots made", shot_pct=75, battery=84, battery_note="~11 games battery left")),
        ("Competition (side button only)", "Times only, full width; badge for umpires",
         screen.competition_screen(COMP_STONES, COMP_ME, COMP_ACTIVE)),
        ("League (+ rocker down)", "Times and your own 16 rocks",
         screen.main_screen(STONES, BASE + [False], tip=TIPS[0])),
        ("Coach, page 1 (+ rocker up)", "Timer priority; double-press the rocker to switch",
         screen.coach_timer_screen(COACH_STONES, COACH_SHOTS)),
        ("Coach, page 2", "Rock priority: all 64 rocks, % per position",
         screen.coach_rocks_screen(COACH_STONES, COACH_SHOTS)),
        ("Competition, low battery", "The badge's second line becomes the warning",
         screen.competition_screen(COMP_STONES, COMP_ME, COMP_ACTIVE, low_battery=True)),
    ], DOCS / "modes.png")

    sheet("Logging your shots with the rocker (ring = cursor on the next slot)", [
        ("1. Rocker UP", "Rock fills with a check, cursor moves on", screen.main_screen(STONES, BASE + [True], tip=TIPS[0])),
        ("2. Rocker UP (by mistake)", "It was actually a miss", screen.main_screen(STONES, BASE + [True, True], tip=TIPS[0])),
        ("3. HOLD rocker 0.5 s", "Undo: cursor steps back and clears that rock", screen.main_screen(STONES, BASE + [True], tip=TIPS[0])),
        ("4. Rocker DOWN", "Rock fills with an X, cursor moves on", screen.main_screen(STONES, BASE + [True, False], tip=TIPS[0])),
    ], DOCS / "rocker-storyboard.png", cols=2)


if __name__ == "__main__":
    main()
