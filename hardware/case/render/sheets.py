"""Lays the rendered views (view-*.png from shoot.js) out as ../preview.png and ../mount-concept.png.
python sheets.py"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import case  # noqa: E402
import trimesh  # noqa: E402

bold = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 30)
reg = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20)


def sheet(views, title, sub, out, cols):
    ims = [Image.open(HERE / f"view-{v}.png").convert("RGB") for v, _ in views]
    w, h = ims[0].size
    w2, h2 = w // 2, h // 2
    cap = 70
    rows = (len(views) + cols - 1) // cols
    s = Image.new("RGB", (w2 * cols, (h2 + cap) * rows + 120), (244, 244, 242))
    d = ImageDraw.Draw(s)
    d.text((30, 25), title, font=bold, fill=(30, 30, 30))
    d.text((30, 70), sub, font=reg, fill=(90, 90, 90))
    for i, (im, (_, label)) in enumerate(zip(ims, views)):
        x, y = (i % cols) * w2, 120 + (i // cols) * (h2 + cap)
        s.paste(im.resize((w2, h2), Image.LANCZOS), (x, y))
        d.text((x + w2 // 2, y + h2 + 12), label, font=reg, fill=(40, 40, 40), anchor="ma")
    s.save(out)
    print("wrote", Path(out).name)


def size(name):
    m = trimesh.load(HERE.parent / f"stoprock-case-{name}.stl")
    e = m.extents
    return f"{e[0]:.1f} x {e[1]:.1f} x {e[2]:.1f} mm, {m.volume / 1000:.1f} cm3"


sheet([("front", "Assembled, front"), ("bezel", "Bezel: faceted front"), ("right", "Right side: rocker cut-out, power pad"),
       ("exploded", "Exploded: tub, CoreInk, bezel"), ("tub", "Tub alone: pocket, USB-C opening, top pad"),
       ("back", "Back: debossed wordmark, reset pinhole")],
      "stoprock case: two TPU parts (tub + bezel) for an M5Stack CoreInk",
      f"Tub {size('tub')}   |   Bezel {size('bezel')}   |   TPU 95A   |   white TPU; the blue-grey block is the CoreInk, for context",
      HERE.parent / "preview.png", 3)
sheet([("cord", "Cord lanyard; a sliding bead or knot closes the V"), ("cord-close", "Close-up: the cord entering the tunnel at the side"),
       ("broom", "Silicone band round a broom handle"), ("broom-side", "The same, from the side")],
      "stoprock case: cord lanyard and silicone broom band",
      "Both go through the same 21 x 2 mm tunnel across the back (2-3 mm cord; 20 x 1 mm band). "
      "The band's bonded overlap: band-section.png",
      HERE.parent / "mount-concept.png", 2)
