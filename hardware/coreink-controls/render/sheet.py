"""Lays the rendered views (view-*.png from shoot.js) out as ../wiring.png.   python sheet.py"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
bold = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 30)
mid = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20)
reg = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 19)
mono = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 18)
VIEWS = [("back", "From behind: header in the back socket, the leads running down"),
         ("back-close", "The header: unused pins snipped, plugs lying flat"),
         ("side", "From the side: about 6 mm off the back"),
         ("front", "From the front: the stick and the timing button below")]
PINS = [("socket", "signal", "lead", "goes to"),
        ("16", "pin 25", "white", "timing button"),
        ("7", "pin 13", "yellow", "stick up"),
        ("8", "pin 14", "green", "stick down"),
        ("14", "pin 26", "blue", "stick left"),
        ("12", "pin 36 *", "purple", "stick right"),
        ("13", "pin 34 *", "orange", "stick centre"),
        ("1", "GND", "black", "stick COM, button"),
        ("6", "3.3 V", "red", "stick VCC *")]
ims = [Image.open(HERE / f"view-{v}.png").convert("RGB") for v, _ in VIEWS]
w, h = ims[0].size[0] // 2, ims[0].size[1] // 2
cap, table_w = 40, 560
s = Image.new("RGB", (w * 2 + table_w, 130 + (h + cap) * 2), (244, 244, 242))
d = ImageDraw.Draw(s)
d.text((30, 25), "CoreInk + 5-way stick + timing button, through the back socket, no solder", font=bold, fill=(30, 30, 30))
d.text((30, 72), "A 2 x 8 right-angle header (Sullins PRPC040DBAN-M71RC, cut) plugged into the back socket, female jumper "
       "plugs on its bent pins, short leads (about 5 cm).", font=reg, fill=(90, 90, 90))
for i, (im, (_, label)) in enumerate(zip(ims, VIEWS)):
    x, y = (i % 2) * w, 120 + (i // 2) * (h + cap)
    s.paste(im.resize((w, h), Image.LANCZOS), (x, y))
    d.text((x + 16, y + h + 6), label, font=reg, fill=(50, 50, 50))
x0, y0 = w * 2 + 20, 140
d.text((x0, y0), "Which pin goes where", font=mid, fill=(30, 30, 30))
for r, row in enumerate(PINS):
    y = y0 + 40 + r * 32
    for c, (cell, cx) in enumerate(zip(row, (0, 110, 230, 320))):
        d.text((x0 + cx, y), cell, font=mid if r == 0 else mono, fill=(30, 30, 30) if r == 0 else (60, 60, 60))
notes = ["The other 8 pins are snipped off the header,",
         "reset (socket pin 5) among them.",
         "* Pins 34 and 36 have no built-in pull-up:",
         "  use a stick module with a VCC pin (pull-ups",
         "  on board), or add two 10 kOhm resistors.",
         "Check the socket's pin order against the",
         "CoreInk's own marking before plugging in."]
for i, line in enumerate(notes):
    d.text((x0, y0 + 40 + len(PINS) * 32 + 20 + i * 26), line, font=reg, fill=(80, 80, 80))
s.save(HERE.parent / "wiring.png")
print("wrote wiring.png")
