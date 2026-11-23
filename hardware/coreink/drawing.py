"""Draws the CoreInk dimension sheet from coreink.json (our own measurements).

    python hardware/coreink/drawing.py   -> hardware/coreink/coreink-dimensions.png
"""
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
D = json.loads((HERE / "coreink.json").read_text())
B = D["body"]
W, H, T = B["width"], B["height"], B["depth"]
K = 9                                    # px per mm
SS = 2                                   # supersampling
INK, DIM, FEAT, FILL = (30, 30, 30), (200, 60, 40), (40, 90, 170), (226, 232, 242)
bold = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 17 * SS)
reg = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 12 * SS)
title = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24 * SS)

img = Image.new("RGB", (2350 * SS, 1420 * SS), (250, 250, 248))
d = ImageDraw.Draw(img)


class View:
    """A view of one face: (u, v) in mm, origin at the face's lower-left as drawn, at sheet (ox, oy)."""

    def __init__(self, ox, oy, w, h, name, ulabel, vlabel, flip_u=False):
        self.ox, self.oy, self.w, self.h, self.flip = ox, oy, w, h, flip_u
        r = B["outline_corner_radius"] * K * SS if name.startswith(("Front", "Back")) else 1.0 * K * SS
        d.rounded_rectangle(self.box(0, 0, w, h), radius=r, outline=INK, width=2 * SS, fill=(255, 255, 255))
        d.text((ox * SS, (oy - 32) * SS), name, font=bold, fill=INK)
        d.text((ox * SS, (oy + h * K + 8) * SS), f"{ulabel} 0-{w:g} ->   {vlabel} 0-{h:g} up", font=reg, fill=(120, 120, 120))

    def p(self, u, v):
        if self.flip:
            u = self.w - u
        return ((self.ox + u * K) * SS, (self.oy + (self.h - v) * K) * SS)

    def box(self, u0, v0, u1, v1):
        (x0, y0), (x1, y1) = self.p(u0, v1), self.p(u1, v0)
        return [min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)]

    def rect(self, u, v, label=None, r=0.8, at=None):
        d.rounded_rectangle(self.box(u[0], v[0], u[1], v[1]), radius=r * K * SS, outline=FEAT, width=2 * SS, fill=FILL)
        if label:
            self.note(u, v, label, at)

    def circle(self, c, dia, label=None, at=None):
        x, y = self.p(*c)
        r = dia / 2 * K * SS
        d.ellipse([x - r, y - r, x + r, y + r], outline=FEAT, width=2 * SS, fill=FILL)
        if label:
            self.note([c[0] - dia / 2, c[0] + dia / 2], [c[1] - dia / 2, c[1] + dia / 2], label, at)

    def note(self, u, v, label, at=None):
        """A label to the right of the view, level with v (mm) unless `at` gives another height;
        the leader runs from the feature's centre."""
        x0, y0, x1, y1 = self.box(u[0], v[0], u[1], v[1])
        fx, fy = (x0 + x1) / 2, (y0 + y1) / 2
        ty = self.p(0, at)[1] if at is not None else fy
        tx = (self.ox + self.w * K + 16) * SS
        d.ellipse([fx - 3 * SS, fy - 3 * SS, fx + 3 * SS, fy + 3 * SS], fill=(110, 110, 110))
        d.line([(fx, fy), (tx - 30 * SS, ty), (tx - 4 * SS, ty)], fill=(140, 140, 140), width=SS)
        d.text((tx, ty - 8 * SS), label, font=reg, fill=INK)


def rng(a):
    return f"{a[0]:g}-{a[1]:g}"


d.text((40 * SS, 24 * SS), "M5Stack CoreInk: dimensions for the stoprock case (mm)", font=title, fill=INK)
d.text((40 * SS, 62 * SS), f"Body {W:g} x {H:g} x {T:g}, outline corners R{B['outline_corner_radius']:g}, "
       f"{B['weight_g']:g} g. x from the left edge as seen from the front, y up from the bottom, z from the back face. "
       "Numbers: coreink.json.", font=reg, fill=(90, 90, 90))

# front
f = D["front"]
v = View(160, 150, W, H, "Front", "x", "y")
v.rect(f["screen_window"]["x"], f["screen_window"]["y"], f"screen window x {rng(f['screen_window']['x'])}, y {rng(f['screen_window']['y'])}")
for c in f["corner_holes"]["centres"]:
    v.circle(c, f["corner_holes"]["diameter"])
v.note([32, 40], [48, 56], f"holes Ø{f['corner_holes']['diameter']:g}, centres 4 from the edges", at=54)
v.rect(f["dial_notch"]["x"], f["dial_notch"]["y"], f"notch round the dial, y {rng(f['dial_notch']['y'])}", r=0.2, at=46)

# back, as seen from behind (mirrored)
bk = D["back"]
v = View(160, 830, W, H, "Back (as seen from behind)", "x (mirrored)", "y", flip_u=True)
v.rect(D["top"]["rear_recess"]["x"], D["top"]["rear_recess"]["y"], f"top recess: x {rng(D['top']['rear_recess']['x'])}, y {rng(D['top']['rear_recess']['y'])}", at=54)
v.rect(bk["reset_button"]["x"], bk["reset_button"]["y"], f"reset: x {rng(bk['reset_button']['x'])}, y {rng(bk['reset_button']['y'])}", r=2.5, at=45)
v.rect(bk["slot"]["x"], bk["slot"]["y"], f"slot: x {rng(bk['slot']['x'])}, y {rng(bk['slot']['y'])}", at=39)
for k, yc in enumerate(bk["small_holes"]["y_centres"]):
    v.rect(bk["small_holes"]["x"], [yc - 0.75, yc + 0.75], None, r=0.1)
v.note(bk["small_holes"]["x"], [29, 43], "6 holes 1.25 x 1.5: x 37.25-38.5, y 29.75-42.25 (2.5 apart)", at=33)
v.circle(bk["buzzer_grille"]["centre"], bk["buzzer_grille"]["outer_diameter"], f"buzzer Ø12, centre {bk['buzzer_grille']['centre'][0]:g}, {bk['buzzer_grille']['centre'][1]:g}", at=27)
v.rect(bk["expansion_header_window"]["x"], bk["expansion_header_window"]["y"], f"2x8 header: x {rng(bk['expansion_header_window']['x'])}, y {rng(bk['expansion_header_window']['y'])}", at=20)
for c in bk["screw_holes"]["centres"]:
    v.circle(c, bk["screw_holes"]["diameter"])
v.note([9, 13], [2, 6], f"screw holes Ø{bk['screw_holes']['diameter']:g} at x 11.1 and 29.1, y 4.1", at=10)
for c in bk["corner_holes"]["centres"]:
    v.circle(c, bk["corner_holes"]["diameter"])
v.note([1, 7.2], [1, 7.2], f"corner holes Ø{bk['corner_holes']['diameter']:g}, centres 4.1 from the edges", at=3)

# right side: z across (back to front), y up
r = D["right"]
v = View(1030, 150, T, H, "Right side", "z", "y")
v.rect(r["dial_window"]["z"], r["dial_window"]["y"], f"dial window: y {rng(r['dial_window']['y'])}, z {rng(r['dial_window']['z'])} (to the front)", r=1.5)
v.rect(r["power_button"]["z"], r["power_button"]["y"], f"power button: y {rng(r['power_button']['y'])}, z {rng(r['power_button']['z'])}", r=3)
d.text((1030 * SS, (150 + H * K + 26) * SS), "back <- -> front", font=reg, fill=(120, 120, 120))

# left side
v = View(1030, 830, T, H, "Left side", "z (mirrored)", "y", flip_u=True)
v.note([0, T], [26, 30], "no openings or controls")

# top and bottom edges: x across, z up (back at the bottom)
t = D["top"]
v = View(1650, 150, W, T, "Top edge", "x", "z (back at the bottom)")
v.rect(t["recessed_panel"]["x"], t["recessed_panel"]["z"], f"recessed panel: x {rng(t['recessed_panel']['x'])}", r=0.5, at=13)
v.rect(t["button"]["x"], t["button"]["z"], f"button: x {rng(t['button']['x'])}, z {rng(t['button']['z'])}", r=1.5, at=4)
bt = D["bottom"]
v = View(1650, 500, W, T, "Bottom edge", "x", "z (back at the bottom)")
v.rect(bt["port_opening"]["x"], bt["port_opening"]["z"], f"USB-C + Grove: x {rng(bt['port_opening']['x'])}, z {rng(bt['port_opening']['z'])}", r=1)

img.resize((img.width // SS, img.height // SS), Image.LANCZOS).save(HERE / "coreink-dimensions.png")
print("wrote coreink-dimensions.png")
