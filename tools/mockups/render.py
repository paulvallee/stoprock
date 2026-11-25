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

BASE = [True, True, False, True, True, True]  # your rocks, ends 1-3 (league)
# coach mode sample: end 5 under way (lead's two rocks and second's first logged)
COACH_SHOTS = [i % 5 != 3 and i % 7 != 5 for i in range(35)]
COACH_STONES = [(3.47, 13.92, 4), (3.29, None, 1), (3.51, 13.66, 4), (None, 14.02, 1), (3.38, None, 4),
                (3.42, 13.81, 0), (3.61, 14.20, 4), (3.33, 13.75, 0), (None, 13.88, 4), (3.40, 14.05, 3),
                (3.55, None, 4), (3.36, 13.97, 3)]  # newest first; player 4 = THEM
COACH_ACTIVE = 1  # the 2nd's second rock is next

# competition and league sample: you're 2nd; your teammates' and the other team's (player 4) stones
# newest first, and the 3rd up next
LEAGUE_ME = 1
LEAGUE_STONES = [(3.41, 13.86, 4), (3.47, 13.92, 2), (3.29, None, 4), (3.51, 13.66, 2), (None, 14.02, 4),
                 (3.38, 13.71, 4), (3.42, 13.81, 0), (3.61, 14.20, 4), (3.33, 13.75, 0), (3.40, 14.05, 4),
                 (3.55, None, 3), (3.36, 13.97, 4), (3.44, 13.62, 3), (3.58, 14.11, 4)]
LEAGUE_ACTIVE = 1


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
    DOCS.mkdir(parents=True, exist_ok=True)
    LOGO.mkdir(parents=True, exist_ok=True)

    wm = logo.wordmark()
    wm.save(LOGO / "stoprock-logo.png")
    dev = logo.device_logo()
    dev.save(LOGO / "stoprock-logo-176px-1bit.png")
    print("wrote", (LOGO / "stoprock-logo.png").relative_to(ROOT))

    header = wm.resize((620, round(wm.height * 620 / wm.width)), Image.LANCZOS).convert("RGB")
    sheet("stoprock screens", [
        ("Switched off", "Shown after power-off; e-ink keeps it with no power",
         screen.off_page(84, 11)),
        ("League", "Teammates' and THEM's times; your rocks and % below",
         screen.league_screen(LEAGUE_STONES, BASE + [False], LEAGUE_ME, LEAGUE_ACTIVE)),
        ("Low battery", "A bar over the percentage until charged",
         screen.league_screen(LEAGUE_STONES, BASE + [False], LEAGUE_ME, LEAGUE_ACTIVE, low_battery=True)),
    ], DOCS / "screens.png", header=header)

    sheet("Start-up modes: hold the side button to switch on, plus the rocker to choose", [
        ("Off page", "Press the side button to wake",
         screen.off_page(84, 11)),
        ("Competition (side button only)", "Times only, by player and THEM; badge for umpires",
         screen.competition_screen(LEAGUE_STONES, LEAGUE_ME, LEAGUE_ACTIVE)),
        ("League (+ rocker down)", "Teammates' and THEM's times; your own 16 rocks",
         screen.league_screen(LEAGUE_STONES, BASE + [False], LEAGUE_ME, LEAGUE_ACTIVE)),
        ("Coach, page 1 (+ rocker up)", "Timer priority: times by position and THEM",
         screen.coach_timer_screen(COACH_STONES, COACH_SHOTS, active=COACH_ACTIVE)),
        ("Coach, page 2", "Rock priority: all 64 rocks, % per position",
         screen.coach_rocks_screen(COACH_STONES, COACH_SHOTS)),
        ("Basic (from the menu)", "Times only, as big as they go: 4 stones, no players",
         screen.basic_screen(LEAGUE_STONES)),
    ], DOCS / "modes.png")

    # you're 2nd: hold the rocker down to log your rocks; after one, the focus is back on the
    # columns and the other team is up next
    def lg(shots, focus="players", active=3):
        return screen.league_screen(LEAGUE_STONES, shots, LEAGUE_ME, active, focus=focus)
    sheet("Logging your shots with the rocker (ring = cursor on your next rock)", [
        ("1. HOLD rocker DOWN 0.5s", "The bold border moves to your rocks", lg(BASE, "rocks", 1)),
        ("2. Rocker UP", "Made (filled); back to the columns, THEM up next", lg(BASE + [True])),
        ("3. HOLD rocker centre 0.5s", "Undo: cursor steps back and clears that rock", lg(BASE)),
        ("4. Hold DOWN, then DOWN", "Rock fills with an X", lg(BASE + [False])),
    ], DOCS / "rocker-storyboard.png", cols=2)


if __name__ == "__main__":
    main()
