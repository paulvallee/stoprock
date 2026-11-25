"""Concept: a broom case for the CoreInk with the 5-way stick and the timing button stacked in a tail below.
For looking at, not printing yet.

    python hardware/coreink-controls/case.py     -> render/model-case-*.stl, render/case.json

The CoreInk sits upside down, so its USB-C port (on its bottom edge) is at the top end of the case, open
for charging, and the tail can hang below. The firmware turns the screen 180 degrees and swaps the dial's
up and down. Its dial and side (power) button end up on the case's left side, both reachable; its top
button ends up under the tail, covered (the stick and the new timing button take over).

Millimetres. X across the face, Y up it (along the handle), Z out of the face (Z = 0 the back). The
CoreInk's features come from hardware/coreink/coreink.json (its own coordinates; see DIMENSIONS.md).
"""
import json
from pathlib import Path

import numpy as np
import trimesh
from manifold3d import CrossSection, JoinType, Manifold

HERE = Path(__file__).resolve().parent
OUT = HERE / "render"
CI = json.loads((HERE.parent / "coreink" / "coreink.json").read_text())
SEG = 64

# ------------------------------------------------------------------ the CoreInk, turned over
DEV_W, DEV_H, DEV_T = CI["body"]["width"], CI["body"]["height"], CI["body"]["depth"]   # 40 x 56 x 16
FIT, WALL = 0.2, 2.5
BACK = 4.0            # the back under the CoreInk's wiring: the saddle and the strap tunnels
WIRING = 5.5          # room behind the CoreInk for the right-angle header and its plugs
LIP = 1.5             # the front frame over the CoreInk's face
EDGE = 2.0            # the front edge's rounding

W = DEV_W + 2 * (FIT + WALL)
T = BACK + WIRING + DEV_T + LIP
DEV_X0 = WALL + FIT                    # the CoreInk's left edge (as the case is seen)
DEV_Z0 = BACK + WIRING                 # its back face

# ------------------------------------------------------------------ the tail: the stick above the button
TAIL_W, TAIL_T = 30.0, 16.0            # as wide as the handle (about 29 mm); thinner than the body
STICK_KNOB, BUTTON = 10.0, 16.0
RAMP = 9.0                             # the face slopes down from the body to the tail over this length
STICK_Y = 13.0 + 10.0 + 2.0 + 8.0      # from the tail's bottom: margin, button well, gap, stick well
BUTTON_Y = 13.0
SHOULDER = STICK_Y + 8.0 + 3.0 + RAMP  # where the body (full thickness) begins
DEV_Y0 = SHOULDER + WALL + FIT         # the CoreInk's bottom (its top edge, it being turned over)
H = DEV_Y0 + DEV_H + FIT + WALL

HANDLE_R, SADDLE_DEPTH = 14.3, 3.0     # 1 1/8" handle (28.6 mm)
STRAP_W = 16.0
TUNNEL_Z = (SADDLE_DEPTH + 0.4, SADDLE_DEPTH + 0.4 + 2.4)
TUNNEL_YS = (DEV_Y0 + 11.0, DEV_Y0 + 49.0)


def dev(x, y):
    """Case coordinates of a point on the CoreInk's face, given in its own coordinates (it is turned over)."""
    return DEV_X0 + (DEV_W - x), DEV_Y0 + (DEV_H - y)


def rounded_rect(w, h, r, cx=0.0, cy=0.0):
    return CrossSection.square((w - 2 * r, h - 2 * r), center=True).offset(
        r, join_type=JoinType.Round, circular_segments=SEG).translate((cx, cy))


def slab(w, h, r, cx, cy, t):
    """A rounded slab t thick, its front edge eased."""
    lower = rounded_rect(w, h, r, cx, cy).extrude(t - EDGE)
    top = rounded_rect(w - 2 * EDGE, h - 2 * EDGE, max(r - EDGE, 0.5), cx, cy).extrude(0.01).translate((0, 0, t - 0.01))
    return Manifold.batch_hull([lower, top])


def body():
    wide = slab(W, H - SHOULDER, 7.0, W / 2, (H + SHOULDER) / 2, T)
    tail = slab(TAIL_W, SHOULDER - RAMP + 6.0, TAIL_W / 2 - 1.0, W / 2, (SHOULDER - RAMP + 6.0) / 2, TAIL_T)
    # the ramp: the face sloping from the body's thickness down to the tail's
    ramp = Manifold.batch_hull([
        Manifold.cube((TAIL_W - 2 * EDGE, 0.01, T - EDGE)).translate((W / 2 - TAIL_W / 2 + EDGE, SHOULDER + 2.0, 0)),
        Manifold.cube((TAIL_W, 0.01, T - EDGE - 1.0)).translate((W / 2 - TAIL_W / 2, SHOULDER + 2.0, 0)),
        Manifold.cube((TAIL_W - 2 * EDGE, 0.01, TAIL_T - 0.5)).translate((W / 2 - TAIL_W / 2 + EDGE, SHOULDER - RAMP, 0)),
        Manifold.cube((TAIL_W, 0.01, TAIL_T - EDGE)).translate((W / 2 - TAIL_W / 2, SHOULDER - RAMP, 0)),
    ])
    shell = wide + tail + ramp
    # the screen window (the CoreInk's own, 28.75 x 28.25), recessed in the frame, with a lead-in
    sx, sy = dev(*CI["front"]["screen_window_centre"])
    win = rounded_rect(28.75, 28.25, 1.5, sx, sy)
    window = Manifold.batch_hull([win.extrude(0.01).translate((0, 0, DEV_Z0 + DEV_T - 0.3)),
                                  win.offset(LIP, join_type=JoinType.Round, circular_segments=SEG).extrude(0.01).translate((0, 0, T + 0.01))])
    # USB-C and Grove, now at the top end: through the top wall, with a chamfer
    (ax, _), (bx, _) = dev(26.25, 0), dev(14.0, 0)
    z0, z1 = DEV_Z0 + 2.3, DEV_Z0 + 12.05
    port = rounded_rect(bx - ax, z1 - z0, 1.5).extrude(WALL + FIT + 2.0).rotate((-90, 0, 0)).translate(((ax + bx) / 2, H - WALL - FIT - 0.5, (z0 + z1) / 2))
    port_bevel = Manifold.batch_hull([
        rounded_rect(bx - ax, z1 - z0, 1.5).extrude(0.01).rotate((-90, 0, 0)).translate(((ax + bx) / 2, H - 1.2, (z0 + z1) / 2)),
        rounded_rect(bx - ax + 2.4, z1 - z0 + 2.4, 2.7).extrude(0.01).rotate((-90, 0, 0)).translate(((ax + bx) / 2, H + 0.01, (z0 + z1) / 2)),
    ])
    # the dial (its right side, now the left): a slot through the wall, open to the front
    (_, dy0), (_, dy1) = dev(40, 45.25), dev(40, 26.75)
    dial = Manifold.cube((WALL + FIT + 2.0, dy1 - dy0, T - (DEV_Z0 + 5.05) + 1.0)).translate((-1.0, dy0, DEV_Z0 + 5.05))
    # the side (power) button: an opening in the left wall
    (_, py0), (_, py1) = dev(40, 18.25), dev(40, 12.0)
    power = rounded_rect(py1 - py0, 10.0, 3.0).extrude(WALL + FIT + 2.0).rotate((0, 90, 0)).translate((-1.0, (py0 + py1) / 2, DEV_Z0 + 8.05))
    # wells round the stick and the button
    wells = []
    for y, d, depth in ((STICK_Y, STICK_KNOB + 6, 2.0), (BUTTON_Y, BUTTON + 4, 1.6)):
        wells.append(Manifold.batch_hull([
            Manifold.cylinder(0.01, d / 2 - depth, circular_segments=SEG).translate((W / 2, y, TAIL_T - depth)),
            Manifold.cylinder(0.01, d / 2 + depth, circular_segments=SEG).translate((W / 2, y, TAIL_T + 0.01)),
        ]))
    saddle = Manifold.cylinder(H + 2, HANDLE_R + 0.3, circular_segments=SEG * 2).rotate((-90, 0, 0)).translate(
        (W / 2, -1, SADDLE_DEPTH - HANDLE_R - 0.3))
    tunnels = [Manifold.cube((W + 2, STRAP_W + 1, TUNNEL_Z[1] - TUNNEL_Z[0])).translate((-1, y - (STRAP_W + 1) / 2, TUNNEL_Z[0]))
               for y in TUNNEL_YS]
    cut = window + port + port_bevel + dial + power + wells[0] + wells[1] + saddle + tunnels[0] + tunnels[1]
    return shell - cut


def stick():
    post = Manifold.cylinder(2.0, 2.2, circular_segments=SEG).translate((W / 2, STICK_Y, TAIL_T - 2.2))
    cap = Manifold.cylinder(3.2, STICK_KNOB / 2, STICK_KNOB / 2 - 0.6, circular_segments=SEG).translate((W / 2, STICK_Y, TAIL_T - 0.6))
    dish = Manifold.sphere(9, circular_segments=SEG).translate((W / 2, STICK_Y, TAIL_T + 2.6 + 9 - 0.5))
    knob = cap - dish
    for a in range(0, 180, 30):
        knob = knob - Manifold.cube((0.8, STICK_KNOB, 0.6), center=True).rotate((0, 0, a)).translate((W / 2, STICK_Y, TAIL_T + 2.2))
    return post + knob


def button():
    return Manifold.batch_hull([
        Manifold.cylinder(0.01, BUTTON / 2, circular_segments=SEG).translate((W / 2, BUTTON_Y, TAIL_T - 1.6)),
        Manifold.cylinder(0.01, BUTTON / 2, circular_segments=SEG).translate((W / 2, BUTTON_Y, TAIL_T + 0.8)),
        Manifold.cylinder(0.01, BUTTON / 2 - 1.0, circular_segments=SEG).translate((W / 2, BUTTON_Y, TAIL_T + 1.5)),
    ])


def handle():
    return Manifold.cylinder(280, HANDLE_R, circular_segments=SEG * 2).rotate((-90, 0, 0)).translate(
        (W / 2, -110, SADDLE_DEPTH - HANDLE_R))


def strap():
    cz = SADDLE_DEPTH - HANDLE_R

    def loop(grow):
        ring = CrossSection.circle(HANDLE_R + grow, circular_segments=SEG * 2).translate((W / 2, cz))
        bar = CrossSection.square((W + 2 * grow - 0.4, TUNNEL_Z[1] - TUNNEL_Z[0] - 0.6 + 2 * grow)).translate(
            (-grow + 0.2, TUNNEL_Z[0] + 0.3 - grow))
        return CrossSection.batch_hull([ring, bar])
    band = loop(1.6) - loop(0.0)
    parts = [band.extrude(STRAP_W).rotate((90, 0, 0)).translate((0, y + STRAP_W / 2, 0)) for y in TUNNEL_YS]
    return parts[0] + parts[1]


def save(m, name):
    mesh = m.to_mesh()
    trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts)).export(OUT / f"model-case-{name}.stl")


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    for name, part in (("body", body()), ("stick", stick()), ("button", button()), ("handle", handle()), ("strap", strap())):
        save(part, name)
    sx, sy = dev(*CI["front"]["screen_window_centre"])
    (OUT / "case.json").write_text(json.dumps(dict(
        W=W, H=H, T=T, screen=[sx, sy, DEV_Z0 + DEV_T + 0.05],
        # the CoreInk model (its own coordinates, offset 2.5 in M5Stack's file) turned over and placed
        device=dict(x0=DEV_X0, y0=DEV_Y0, z0=DEV_Z0, w=DEV_W, h=DEV_H),
        tail=dict(stick_y=STICK_Y, button_y=BUTTON_Y, t=TAIL_T)), indent=1))
    print(f"case {W:.1f} x {H:.1f} x {T:.1f} mm (tail {TAIL_W:.0f} wide, {TAIL_T:.0f} thick)")
