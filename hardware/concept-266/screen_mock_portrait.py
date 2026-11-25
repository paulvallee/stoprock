"""A rough 152 x 296 (portrait) screen for the vertical concept: the newest stone big at the top
(B2H, then H2H white on black), then the stones before it a row each, newest first, with who threw
them. Not a designed layout, just our fonts at the new size.   python screen_mock_portrait.py"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "tools" / "mockups"))
import screen  # noqa: E402

W, H = 152, 296
f = screen.fonts()
img = Image.new("1", (W, H), 1)
d = ImageDraw.Draw(img)
d.fontmode = "1"
screen.dseg_number(d, 3.47, f["big"], 0, W, 5, 47, margin=0, align="center")
d.rectangle((0, 52, W - 1, 99), fill=0)
screen.dseg_number(d, 13.92, f["big"], 0, W, 57, 95, margin=0, align="center", fill=1)
rows = [("3rd", 3.52, 14.01), ("THEM", 3.29, 13.66), ("THEM", 3.44, 13.95), ("2nd", 3.41, 13.86),
        ("2nd", 3.38, 13.79), ("THEM", None, 14.12), ("THEM", 3.50, 13.90)]
top, pitch = 103, 27
for i, (who, b, h) in enumerate(rows):
    y = top + i * pitch
    d.line((0, y + pitch - 1, W - 1, y + pitch - 1), fill=0)
    screen.text_in_box(d, who, f["label"], 0, 30, y, y + pitch - 2, align="left", margin=1)
    if b is not None:
        screen.dseg_number(d, b, f["pct"], 28, 72, y + 4, y + 20, margin=2, align="right")
    if h is not None:
        screen.dseg_number(d, h, f["top_small"], 70, W, y + 1, y + 24, margin=2, align="right")
img.save(HERE / "render" / "screen-portrait.png")
img.resize((W * 3, H * 3), Image.NEAREST).save(HERE / "render" / "screen-portrait-3x.png")
print("wrote render/screen-portrait.png")
