"""A rough 540 x 960 screen for the PaperDIY concept (4.7", upright): the newest stone big at the top
(B2H, then H2H white on black), then the stones before it a row each, newest first, with who threw
them. Not a designed layout, just our fonts at this size.   python screen_mock_paperdiy.py"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "tools" / "mockups"))
import screen  # noqa: E402
from logo import NUNITO  # noqa: E402

W, H = 540, 960


def dseg(size):
    return ImageFont.truetype(str(screen.DSEG), size)


def nunito(size, weight="ExtraBold"):
    f = ImageFont.truetype(str(NUNITO), size)
    f.set_variation_by_name(weight)
    return f


def number(d, val, font, x0, x1, y0, y1, align="center", margin=0, fill=0):
    """A DSEG number with a square decimal point, measured with the font's own metrics (the shared
    mockup helper measures on a canvas sized for the device's fonts, too small at these sizes)."""
    ip, fp = f"{val:.2f}".split(".")
    li, ti, ri, bi = font.getbbox(ip)
    lf, tf, rf, bf = font.getbbox(fp)
    t, b = min(ti, tf), max(bi, bf)
    dot, gap = max(3, round((b - t) * 0.12)), max(2, round((b - t) * 0.05))
    total = (ri - li) + gap + dot + gap + (rf - lf)
    x = x1 - margin - total if align == "right" else (x0 + x1 - total) / 2
    y = (y0 + y1 - (b - t)) / 2 - t
    d.text((round(x - li), round(y)), ip, font=font, fill=fill)
    xd = x + (ri - li) + gap
    d.rectangle([round(xd), round(y + b - dot), round(xd + dot - 1), round(y + b - 1)], fill=fill)
    d.text((round(xd + dot + gap - lf), round(y)), fp, font=font, fill=fill)


img = Image.new("1", (W, H), 1)
d = ImageDraw.Draw(img)
d.fontmode = "1"
number(d, 3.47, dseg(150), 0, W, 22, 196, margin=0, align="center")
d.rectangle((0, 214, W - 1, 404), fill=0)
number(d, 13.92, dseg(150), 0, W, 222, 396, margin=0, align="center", fill=1)
rows = [("3rd", 3.52, 14.01), ("THEM", 3.29, 13.66), ("THEM", 3.44, 13.95), ("2nd", 3.41, 13.86),
        ("2nd", 3.38, 13.79), ("THEM", None, 14.12), ("THEM", 3.50, 13.90), ("Lead", 3.47, 13.84)]
top, pitch = 416, 68
for i, (who, b, h) in enumerate(rows):
    y = top + i * pitch
    d.line((0, y + pitch - 2, W - 1, y + pitch - 2), fill=0, width=2)
    screen.text_in_box(d, who, nunito(30), 0, 120, y, y + pitch - 4, align="left", margin=12)
    if b is not None:
        number(d, b, dseg(40), 120, 250, y + 12, y + 52, margin=6, align="right")
    if h is not None:
        number(d, h, dseg(54), 250, W, y + 6, y + 58, margin=14, align="right")
img.save(HERE / "render" / "screen-paperdiy.png")
img.resize((W // 2, H // 2), Image.LANCZOS).save(HERE / "render" / "screen-paperdiy-half.png")
print("wrote render/screen-paperdiy.png")
