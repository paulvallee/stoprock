"""Lays the rendered views (view-*.png from shoot.js) out as ../concept.png.   python sheet.py"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
bold = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 30)
reg = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20)
VIEWS = [("front", "Face: the screen on top, the stick and button below"),
         ("face", "Straight on: 84 x 80 mm"),
         ("broom", "On the broom, strapped through two tunnels"),
         ("broom-hand", "From the hand: the stick for the thumb, the big button to time"),
         ("side", "Side: 16 mm thick, a saddle on the back sits on the handle"),
         ("back", "Back: the saddle along the handle")]
ims = [Image.open(HERE / f"view-{v}.png").convert("RGB") for v, _ in VIEWS]
w, h = ims[0].size
w2, h2, cap, cols = w // 2, h // 2, 60, 3
rows = (len(ims) + cols - 1) // cols
s = Image.new("RGB", (w2 * cols, (h2 + cap) * rows + 120), (244, 244, 242))
d = ImageDraw.Draw(s)
d.text((30, 25), "stoprock 2.66\" concept, broom-mounted", font=bold, fill=(30, 30, 30))
d.text((30, 70), "296 x 152 e-paper (60 x 31 mm active), Alps 5-way stick, one timing button, USB-C at the top end."
       " The screen shows a rough mock, not a designed layout.", font=reg, fill=(90, 90, 90))
for i, (im, (_, label)) in enumerate(zip(ims, VIEWS)):
    x, y = (i % cols) * w2, 120 + (i // cols) * (h2 + cap)
    s.paste(im.resize((w2, h2), Image.LANCZOS), (x, y))
    d.text((x + 20, y + h2 + 8), label, font=reg, fill=(50, 50, 50))
s.save(HERE.parent / "concept.png")
print("wrote concept.png")
