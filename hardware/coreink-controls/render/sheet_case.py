"""Lays the case views (view-case-*.png from shoot-case.js) out as ../case.png.   python sheet_case.py"""
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
C = json.loads((HERE / "case.json").read_text())
bold = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 30)
reg = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 19)
VIEWS = [("front", "The face: the stick above the timing button"),
         ("top", "The top end: USB-C (and Grove) open for charging"),
         ("left", "Left side: the dial's slot and the side button"),
         ("broom-hand", "On the broom, from the hand"),
         ("side", "Side: 27 mm body, 16 mm tail, on the handle"),
         ("back", "Back: the saddle and the strap tunnels"),
         ("xray", "Inside: the CoreInk upside down, the controls below"),
         ("broom", "On the broom")]
ims = [Image.open(HERE / f"view-case-{v}.png").convert("RGB") for v, _ in VIEWS]
w, h = ims[0].size[0] // 2, ims[0].size[1] // 2
cols, cap = 4, 40
s = Image.new("RGB", (w * cols, 125 + (h + cap) * 2), (244, 244, 242))
d = ImageDraw.Draw(s)
d.text((30, 25), f"CoreInk broom case with the stick and timing button stacked ({C['W']:.0f} x {C['H']:.0f} x {C['T']:.0f} mm)",
       font=bold, fill=(30, 30, 30))
d.text((30, 72), "The CoreInk sits upside down so its USB-C port is at the top; the firmware turns the screen 180 degrees "
       "and swaps the dial's up and down. Wiring as in wiring.png.", font=reg, fill=(90, 90, 90))
for i, (im, (_, label)) in enumerate(zip(ims, VIEWS)):
    x, y = (i % cols) * w, 115 + (i // cols) * (h + cap)
    s.paste(im.resize((w, h), Image.LANCZOS), (x, y))
    d.text((x + 16, y + h + 6), label, font=reg, fill=(50, 50, 50))
s.save(HERE.parent / "case.png")
print("wrote case.png")
