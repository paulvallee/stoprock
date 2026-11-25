"""Draw the one-bit illustrations used by the on-device Tutorials menu."""

from functools import cache
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from fontgen import emit_bitmap, glyph, logo, screen

WIDTH, HEIGHT = 200, 127
OUT = Path(__file__).resolve().parents[1] / "firmware/lib/stoprock_ui/src/tutorial_data.cpp"


@cache
def font(size, weight="Bold"):
    face = ImageFont.truetype(str(logo.NUNITO), size)
    face.set_variation_by_name(weight)
    return face


def label(draw, text, box, face, ink=0):
    screen.text_in_box(draw, text, face, *box, align="center", fill=ink)


def pill(draw, text, x, y, width, height, face, black=False):
    draw.rounded_rectangle((x, y, x + width - 1, y + height - 1), radius=5,
                           outline=0, width=1, fill=0 if black else 1)
    label(draw, text, (x, x + width, y, y + height), face, int(black))


def arrow(draw, start, end, head=6):
    from math import hypot

    dx, dy = end[0] - start[0], end[1] - start[1]
    length = hypot(dx, dy)
    ux, uy = dx / length, dy / length
    base = (end[0] - head * ux, end[1] - head * uy)
    draw.line((start, tuple(round(v) for v in base)), fill=0, width=2)
    draw.polygon((end, (round(base[0] - head * uy * .65), round(base[1] + head * ux * .65)),
                       (round(base[0] + head * uy * .65), round(base[1] - head * ux * .65))), fill=0)


def rocker(draw, x, y, active):
    for i, key in enumerate(("up", "centre", "down")):
        top = y + 24 * i
        on = key in active
        draw.rounded_rectangle((x, top, x + 30, top + 21), radius=4, width=1,
                               outline=0, fill=0 if on else 1)
        ink = int(on)
        if key == "centre":
            draw.ellipse((x + 12, top + 7, x + 18, top + 13), fill=ink)
        else:
            peak, base = (6, 14) if key == "up" else (14, 6)
            draw.polygon(((x + 15, top + peak), (x + 10, top + base), (x + 20, top + base)), fill=ink)


def timing_buttons(draw, small):
    # The hardware diagram supplies the top button's offset and the side button's vertical position.
    import json

    data = json.loads((screen.ROOT / "hardware/coreink/coreink.json").read_text())
    outlined = (71, 11, 128, 84)
    x0, y0, x1, y1 = outlined
    width, height = data["body"]["width"], data["body"]["height"]
    top_x = x0 + round(data["top"]["button"]["centre"][0] * (x1 - x0) / width)
    side_y = y0 + round((height - data["right"]["power_button"]["centre"][0]) * (y1 - y0) / height)
    draw.rounded_rectangle(outlined, radius=7, outline=0, width=2)
    draw.rounded_rectangle((x0 + 7, y0 + 15, x1 - 7, y1 - 12), radius=3, outline=0)
    draw.rounded_rectangle((top_x - 5, y0 - 3, top_x + 5, y0 + 1), radius=2, fill=0)
    draw.rounded_rectangle((x1 - 1, side_y - 5, x1 + 4, side_y + 5), radius=2, fill=0)
    screen.stopwatch(draw, 100, 47, screen.STOPWATCH_SMALL)
    label(draw, "TOP", (5, 57, 9, 32), small)
    arrow(draw, (49, 19), (top_x - 7, 9))
    label(draw, "SIDE", (144, 197, side_y + 3, side_y + 24), small)
    arrow(draw, (162, side_y + 4), (x1 + 6, side_y))


def splits(draw, small):
    for i, (line, action) in enumerate((("BACK", "Start"), ("NEAR", "B2H"), ("FAR", "H2H"))):
        x = 36 + 64 * i
        draw.ellipse((x - 6, 28, x + 6, 40), fill=0)
        label(draw, line, (x - 25, x + 25, 4, 26), small)
        pill(draw, action, x - 25, 50, 50, 23, small, black=i > 0)
        if i < 2:
            arrow(draw, (x + 11, 34), (x + 53, 34))


def basic_history(draw, small):
    rocker(draw, 153, 4, ("up", "down"))
    for row, (a, b) in enumerate((("3.43", "13.91"), ("3.56", "14.12"), ("3.42", "13.60"))):
        y = 6 + row * 23
        pill(draw, a, 14, y, 52, 20, small)
        pill(draw, b, 69, y, 63, 20, small, black=True)


def columns(draw, small, total):
    for i, name in enumerate(("3rd", "Skip", "THEM")):
        x = 12 + 60 * i
        pill(draw, name, x, 11, 56, 22, small, black=i == 2)
        label(draw, ("3.42", "3.65", "3.52")[i], (x, x + 56, 38, 62), small)
    for i in range(total):
        x0, x1 = 12 + 176 * i // total, 12 + 176 * (i + 1) // total - 3
        draw.rectangle((x0, 69, x1, 72) if i >= total - 3 else (x0, 70, x1, 70), fill=0)


def history(draw, small):
    rocker(draw, 153, 5, ("up", "down"))
    pill(draw, "NEWER", 18, 8, 98, 22, small)
    pill(draw, "OLDER", 18, 55, 98, 22, small, black=True)
    arrow(draw, (62, 34), (62, 51))


def shot_logging(draw, small):
    for i, stage in enumerate(("Times", "History", "Rocks")):
        x = 6 + 66 * i
        pill(draw, stage, x, 8, 56, 22, small, black=i == 2)
        if i < 2:
            arrow(draw, (x + 56, 19), (x + 65, 19), head=4)
    for x, made in ((62, True), (138, False)):
        draw.ellipse((x - 11, 42, x + 11, 64), outline=0, width=2, fill=0 if made else 1)
        if not made:
            draw.ellipse((x - 3, 50, x + 3, 56), fill=0)
        label(draw, "UP MADE" if made else "DOWN MISS", (x - 38, x + 38, 67, 83), small)


def position(draw, small):
    for i, name in enumerate(("Lead", "2nd", "3rd", "Skip")):
        pill(draw, name, 7 + i * 47, 45, 44, 23, small, black=i == 0)
    label(draw, "••", (0, 200, 3, 38), font(27))
    arrow(draw, (24, 76), (176, 76))


def coach_pages(draw, small):
    for x, title, mode in ((11, "Times", 0), (112, "Rocks", 1)):
        draw.rounded_rectangle((x, 9, x + 76, 59), radius=5, outline=0, width=2)
        label(draw, title, (x, x + 77, 29, 55), small)
        for mark in range(2):
            dx = x + 60 + mark * 7
            draw.rectangle((dx, 14, dx + 4, 18), outline=0, fill=0 if mark == mode else 1)
    arrow(draw, (92, 36), (107, 36))
    label(draw, "••", (0, 200, 60, 88), font(23))


def undo(draw, small):
    rocker(draw, 32, 5, ("centre",))
    pill(draw, "HOLD 1 SEC", 91, 25, 98, 30, small, black=True)
    arrow(draw, (87, 40), (65, 40))


# Body artwork is shared when the same control works in several modes.
SLIDES = {
    "Buttons": ("Timing buttons", timing_buttons,
                ("Tap either timer button", "to start or record a split.")),
    "Splits": ("Time a stone", splits,
               ("Tap at the back line, then", "at the near and far hogs.")),
    "BasicHistory": ("Browse times", basic_history,
                    ("Tap down for older stones.", "Tap up for newer ones.")),
    "Columns": ("Choose a player", lambda d, f: columns(d, f, 4),
                ("Tap up or down to move.", "Hold up to refile a time.")),
    "CoachColumns": ("Choose a player", lambda d, f: columns(d, f, 5),
                     ("Tap up or down to move.", "Hold up to refile a time.")),
    "History": ("Scroll through times", history,
                ("Hold down to open.", "Tap up/down to scroll.", "Hold up or tap a timer.")),
    "ShotLogging": ("Log made or missed", shot_logging,
                    ("Hold down again for rocks.", "Hold up or tap a timer.")),
    "Position": ("Change your position", position,
                 ("Double-tap the rocker", "centre to change position.")),
    "CoachPages": ("Switch Coach views", coach_pages,
                   ("Double-tap the centre to", "switch to the full rock log.")),
    "Undo": ("Undo an entry", undo,
             ("Hold the rocker centre", "to undo the latest entry.")),
}
# Match the GameMode enum order in databank.h.
DECKS = (
    ("Buttons", "Splits", "Columns", "History", "ShotLogging", "Position", "Undo"),
    ("Buttons", "Splits", "Columns", "History", "Position", "Undo"),
    ("Buttons", "Splits", "CoachColumns", "History", "ShotLogging", "CoachPages", "Undo"),
    ("Buttons", "Splits", "BasicHistory", "Undo"),
)
MODES = ("League", "Competition", "Coach", "Basic")


def body(key):
    _, diagram, copy = SLIDES[key]
    image = Image.new("1", (WIDTH, HEIGHT), 1)
    draw = ImageDraw.Draw(image)
    draw.fontmode = "1"
    diagram(draw, font(13, "ExtraBold"))
    copy_face = font(15)
    pitch = 13 if len(copy) > 2 else 18
    for i, line in enumerate(copy):
        top = 85 + pitch * i
        assert screen.ink(line, copy_face)[2] - screen.ink(line, copy_face)[0] < WIDTH
        label(draw, line, (5, WIDTH - 5, top, top + pitch), copy_face)
    return image


def heading():
    image = Image.new("1", (WIDTH, 39), 1)
    draw = ImageDraw.Draw(image)
    draw.fontmode = "1"
    label(draw, "Tutorials", (0, WIDTH, 0, 39), font(30, "ExtraBold"))
    return image


def page_image(mode, page):
    name, deck = MODES[mode], DECKS[mode]
    title = SLIDES[deck[page]][0]
    faces = screen.fonts()
    detail, headline = faces["menu_detail"], faces["menu_title"]
    image = Image.new("1", (200, 200), 1)
    draw = ImageDraw.Draw(image)
    draw.fontmode = "1"

    def bounds(text, face):
        pen, edges = 0, []
        for char in text:
            dx, _, width, _, advance, _ = glyph(face, char)
            if width:
                edges.append((pen + dx, pen + dx + width))
            pen += advance
        return min(a for a, _ in edges), max(b for _, b in edges)

    screen.glyphs(draw, name, detail, 10, 4)
    progress = f"{page + 1}/{len(deck)}"
    screen.glyphs(draw, progress, detail, 190 - bounds(progress, detail)[1], 4)
    left, right = bounds(title, headline)
    assert right - left <= 190, title
    screen.glyphs(draw, title, headline, (200 - (right - left)) // 2 - left, 20)
    draw.line((10, 42, 189, 42), fill=0)
    image.paste(body(deck[page]), (0, 43))
    draw.rectangle((0, 174, 199, 199), fill=0)
    screen.glyphs(draw, "Up/down: pages", detail, 10, 180, 1)
    back = "Centre: back"
    screen.glyphs(draw, back, detail, 190 - bounds(back, detail)[1], 180, 1)
    return image


def main():
    lines = [
        "// Generated by tools/tutorials.py. Do not edit by hand.",
        '#include "tutorials.h"',
        "",
        "namespace stoprock {",
        "",
    ]
    emit_bitmap("kTutorialsHeading", heading(), lines)
    for key in SLIDES:
        emit_bitmap("kTutorial" + key, body(key), lines)
    lines.append("const TutorialSlide kTutorialSlides[] = {")
    for key, (title, _, _) in SLIDES.items():
        lines.append(f'    {{"{title}", kTutorial{key}}},')
    lines.append("};")
    for i, deck in enumerate(DECKS):
        ids = ", ".join(str(tuple(SLIDES).index(key)) for key in deck)
        lines.append(f"static const uint8_t kDeck{i}[] = {{{ids}}};")
    lines.append("const TutorialDeck kTutorialDecks[] = {")
    for i in range(len(DECKS)):
        lines.append(f"    {{kDeck{i}, sizeof kDeck{i}}},")
    lines.extend(("};", "", "}  // namespace stoprock", ""))
    OUT.write_text("\n".join(lines))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
