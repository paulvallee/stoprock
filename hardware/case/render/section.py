"""A section through the strap tunnel, with the case on a broom: shows the silicone band's
bonded overlap lying doubled inside the tunnel.   python section.py -> ../band-section.png"""
from pathlib import Path
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import case  # noqa: E402
from manifold3d import Manifold  # noqa: E402

cy = case.TUNNEL["cy"]
dev = Manifold.cube((case.DEV_W, case.DEV_H, case.DEV_T)).translate((case.WALL, case.WALL, case.DEV_Z0))
handle = case.scene_parts()["handle"]
parts = [  # drawn in this order
    ("handle", handle, (201, 164, 106)),
    ("tub", case.tub(), (59, 63, 70)),
    ("bezel", case.bezel(), (106, 113, 123)),
    ("device", dev, (225, 225, 220)),
    ("band", case.broom_band(), (217, 84, 43)),
]


def section(m):
    """Polygons of m cut by the plane y = cy, as (x, z) points."""
    return [[(x, -yz) for x, yz in poly] for poly in m.rotate((90, 0, 0)).slice(cy).to_polygons()]


def draw(scale, x0, x1, z0, z1, pad=30):
    W, H = int((x1 - x0) * scale) + 2 * pad, int((z1 - z0) * scale) + 2 * pad
    img = Image.new("RGB", (W, H), (244, 244, 242))
    px = lambda x, z: (pad + (x - x0) * scale, pad + (z1 - z) * scale)   # noqa: E731  (front up)
    for _, m, col in parts:
        mask = Image.new("1", (W, H), 0)
        for poly in section(m):
            one = Image.new("1", (W, H), 0)
            ImageDraw.Draw(one).polygon([px(x, z) for x, z in poly], fill=1)
            mask = ImageChops.logical_xor(mask, one)
        img.paste(Image.new("RGB", (W, H), col), (0, 0), mask)
    return img, px


bold = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 26)
reg = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 19)
small = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 17)

whole, pw = draw(11, -6, case.CW + 6, -33, 26)
zoom, pz = draw(34, -3, 24, -3.8, 4.4)
LAB = 270            # room for labels to the right of each drawing
sheet = Image.new("RGB", (whole.width + LAB + zoom.width + 360 + 40, max(whole.height, zoom.height) + 170), (244, 244, 242))
d = ImageDraw.Draw(sheet)
d.text((30, 22), "Silicone band on a broom: section through the strap tunnel", font=bold, fill=(30, 30, 30))
d.text((30, 62), "The band's two ends overlap and are bonded; the overlap lies doubled inside the tunnel, "
       "which hides and clamps the joint.", font=reg, fill=(90, 90, 90))
wx, wy = 20, 120
zx, zy = whole.width + LAB + 40, 150
sheet.paste(whole, (wx, wy))
sheet.paste(zoom, (zx, zy))
d.text((zx, zy - 38), "Close-up: the left half of the tunnel (the band continues to the right)", font=reg, fill=(40, 40, 40))


def label(base, pxf, img_w, x, z, text):
    """A dot on the feature, a horizontal leader to the right margin, and the label."""
    ax, ay = pxf(x, z)
    ax, ay = ax + base[0], ay + base[1]
    ex = base[0] + img_w + 10
    d.line([(ax, ay), (ex, ay)], fill=(110, 110, 110), width=2)
    d.ellipse([ax - 4, ay - 4, ax + 4, ay + 4], fill=(70, 70, 70))
    d.text((ex + 8, ay - 11), text, font=small, fill=(30, 30, 30))


W1 = whole.width
label((wx, wy), pw, W1, case.CW - 4, 20.2, "bezel")
label((wx, wy), pw, W1, case.CW - 6, 12, "CoreInk")
label((wx, wy), pw, W1, case.CW - 0.5, 6, "case (tub)")
label((wx, wy), pw, W1, case.CW - 5, -0.2, "tunnel, with the band")
label((wx, wy), pw, W1, case.CW + 1.0, -5, "silicone band, 1 mm")
label((wx, wy), pw, W1, case.CW / 2 + 6, -16, "broom handle")
W2 = zoom.width
label((zx, zy), pz, W2, 22, 3.6, "CoreInk")
label((zx, zy), pz, W2, 22, 2.0, "case floor, under the CoreInk")
label((zx, zy), pz, W2, 22, 0.95, "overlap: the other end, bonded")
label((zx, zy), pz, W2, 22, -0.05, "band (first layer)")
label((zx, zy), pz, W2, 22, -1.3, "tunnel floor (outer skin)")
label((zx, zy), pz, W2, 12.5, -2.7, "rail")
label((zx, zy), pz, W2, 22, -3.5, "broom handle")
sheet.save(HERE.parent / "band-section.png")
print("wrote band-section.png", sheet.size)
