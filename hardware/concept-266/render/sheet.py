"""Lays the rendered views (view-<layout>-<view>.png from shoot.js) out as ../concept-portrait.png
(the two portrait layouts, a row each) and ../concept-landscape.png.   python sheet.py"""
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
bold = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 30)
mid = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
reg = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20)
L = json.loads((HERE / "layouts.json").read_text())
NAMES = {"stack": "Stacked: the stick above the timing button", "row": "Side by side: the stick and the timing button",
         "landscape": "Landscape: the screen across the top"}


def sheet(layouts, views, out, title, sub, scale=2):
    first = Image.open(HERE / f"view-{layouts[0]}-{views[0]}.png")
    w, h = first.width // scale, first.height // scale
    cap = 50
    s = Image.new("RGB", (w * len(views), 120 + (h + cap) * len(layouts)), (244, 244, 242))
    d = ImageDraw.Draw(s)
    d.text((30, 25), title, font=bold, fill=(30, 30, 30))
    d.text((30, 70), sub, font=reg, fill=(90, 90, 90))
    for r, lay in enumerate(layouts):
        y = 120 + r * (h + cap)
        size = L[lay]
        d.text((30, y + 10), f"{NAMES[lay]}  ({size['W']:.0f} x {size['H']:.0f} x {size['T']:.0f} mm)",
               font=mid, fill=(40, 40, 40))
        for i, v in enumerate(views):
            im = Image.open(HERE / f"view-{lay}-{v}.png").convert("RGB").resize((w, h), Image.LANCZOS)
            s.paste(im, (i * w, y + cap))
    s.save(HERE.parent / out)
    print("wrote", out)


sheet(["stack", "row"], ["face", "front", "broom", "broom-hand", "side"], "concept-portrait.png",
      "stoprock 2.66\" concept, vertical, on the broom",
      "296 x 152 e-paper turned upright (31 x 60 mm active), Alps 5-way stick and the timing button at the bottom,"
      " USB-C at the top. The screen is a rough mock, not a designed layout.", scale=3)
sheet(["landscape"], ["front", "face", "broom", "broom-hand", "side"], "concept-landscape.png",
      "stoprock 2.66\" concept, landscape (the first look)", "The screen across the top; the stick and button below.", scale=3)
