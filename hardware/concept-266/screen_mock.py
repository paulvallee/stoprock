"""A rough 296 x 152 screen for the concept renders: the newest stone big across the top, three
player columns below. Not a designed layout, just our fonts at the new size.   python screen_mock.py"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "tools" / "mockups"))
import screen  # noqa: E402

W, H = 296, 152
f = screen.fonts()
img = Image.new("1", (W, H), 1)
d = ImageDraw.Draw(img)
d.fontmode = "1"
# the newest stone: B2H black on white, H2H white on black
d.rectangle((W // 2, 0, W - 1, 55), fill=0)
screen.dseg_number(d, 3.47, f["big"], 0, W // 2, 4, 51, margin=0, align="center")
screen.dseg_number(d, 13.92, f["big"], W // 2, W, 4, 51, margin=0, align="center", fill=1)
d.line((0, 56, W - 1, 56), fill=0, width=2)
# three columns: the teammate before, THEM, the one under way
cols = (("2nd", [(3.41, 13.86), (3.52, 14.01), (3.38, 13.79)]),
        ("THEM", [(3.29, 13.66), (3.44, 13.95), (None, 14.12)]),
        ("3rd", [(3.50, 13.90), (3.46, None), (3.61, 14.20)]))
cw = W // 3
for i, (name, rows) in enumerate(cols):
    x0 = i * cw
    if i:
        d.line((x0, 58, x0, H - 1), fill=0)
    active = name == "3rd"
    if active:
        d.rectangle((x0 + 1, 58, x0 + cw - 1, 83), fill=0)
    screen.text_in_box(d, name, f["hint"], x0, x0 + cw, 59, 82, align="center", fill=1 if active else 0)
    for r, (b, h) in enumerate(rows):
        y = 88 + r * 21
        if h is not None:  # the history: H2H only, at this size
            screen.dseg_number(d, h, f["pct"], x0, x0 + cw, y, y + 16, margin=0, align="center")
img.save(HERE / "render" / "screen.png")
img.resize((W * 3, H * 3), Image.NEAREST).save(HERE / "render" / "screen-3x.png")
print("wrote render/screen.png")
