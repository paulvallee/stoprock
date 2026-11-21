"""The stoprock wordmark: Nunito Black lettering, both o's drawn as curling rocks (side view)."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
NUNITO = ROOT / "assets" / "fonts" / "Nunito[wght].ttf"
F = 400  # working size; the result is scaled down


def _font():
    f = ImageFont.truetype(str(NUNITO), F)
    f.set_variation_by_name("Black")
    return f


def _rock(d, x0, w, yb, xh, asc, band):
    """Squat body with a white running band; handle post rising and turning forward into the grip."""
    bw = w * 0.84
    bx0 = x0 + (w - bw) / 2
    top = yb - xh
    d.rounded_rectangle([bx0, top, bx0 + bw, yb + xh * 0.02], radius=xh * 0.46, fill=0)
    bh = xh * band
    by = top + xh * 0.5
    d.rectangle([bx0 - 2, by - bh / 2, bx0 + bw + 2, by + bh / 2], fill=255)
    t = xh * 0.19
    xc = x0 + w / 2
    ytop = yb - asc * 0.97
    post_x0 = xc - t / 2
    d.rectangle([post_x0, ytop + t, post_x0 + t, top + t], fill=0)
    d.rounded_rectangle([post_x0, ytop, xc + w * 0.50, ytop + t], radius=t / 2, fill=0)
    d.pieslice([post_x0, ytop, post_x0 + 2 * t, ytop + 2 * t], 180, 270, fill=0)


def wordmark(band=0.085):
    """Black-on-white wordmark, greyscale ('L') image, cropped to the ink."""
    f = _font()
    _, xt, _, yb = f.getbbox("x")
    _, kt, _, _ = f.getbbox("k")
    xh, asc = yb - xt, yb - kt
    o_w = f.getlength("o") * 1.36
    widths = [o_w if ch == "o" else f.getlength(ch) for ch in "stoprock"]
    track, pad = -F * 0.01, int(F * 0.15)
    img = Image.new("L", (int(sum(widths) + track * 7 + pad * 2), int(yb + F * 0.12 + pad)), 255)
    d = ImageDraw.Draw(img)
    x = pad
    for ch, w in zip("stoprock", widths):
        if ch == "o":
            _rock(d, x, w, yb, xh, asc, band)
        else:
            d.text((x, 0), ch, font=f, fill=0)
        x += w + track
    l, t, r, b = Image.eval(img, lambda v: 255 - v).getbbox()
    m = int(F * 0.06)
    return img.crop((l - m, t - m, r + m, b + m))


def device_logo(width=176):
    """1-bit version for the screen: thicker band so it survives the reduction."""
    wm = wordmark(band=0.14)
    small = wm.resize((width, round(wm.height * width / wm.width)), Image.LANCZOS)
    return small.point(lambda v: 0 if v < 128 else 255).convert("1")
