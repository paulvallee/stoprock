"""Concept: a broom-mounted stoprock with a 2.66" e-paper screen (296 x 152), an Alps 5-way stick and
one big timing button on the face, like a Rockwatcher. For looking at, not printing yet.

    pip install manifold3d trimesh
    python hardware/concept-266/concept.py      -> render/model-*.stl (then render/: see its README)

Millimetres. X across the face, Y up it (along the broom handle), Z out of the face (Z = 0 the back).
"""
from pathlib import Path

import numpy as np
import trimesh
from manifold3d import CrossSection, Manifold

HERE = Path(__file__).resolve().parent
OUT = HERE / "render"
SEG = 64

# ------------------------------------------------------------------ the parts inside
GLASS_W, GLASS_H = 71.82, 36.30      # Good Display 2.66" outline (GDEY0266T90)
ACTIVE_W, ACTIVE_H = 60.09, 30.70    # its active area (296 x 152); taken as centred for the concept
STICK_KNOB = 11.0                    # the 5-way stick's cap (Alps SKRH-type switch underneath)
BUTTON = 18.0                        # the timing button's cap

# ------------------------------------------------------------------ the case
W, H, T = 84.0, 80.0, 16.0           # face width, height, and thickness
R = 10.0                             # corner radius (plan)
EDGE = 2.5                           # the front edge's chamfer-round
SCREEN_CY = H - 6.0 - GLASS_H / 2    # the glass sits 6 mm below the top
WINDOW = (ACTIVE_W + 1.2, ACTIVE_H + 1.2)
WINDOW_DEPTH = 1.6                   # the window is recessed: a face-down drop lands on the frame
CONTROLS_Y = 19.0                    # the row along the bottom
STICK_X, BUTTON_X = 22.0, 61.0

# ------------------------------------------------------------------ the broom
HANDLE_R = 14.5                      # about 29 mm (1 1/8")
SADDLE_DEPTH = 3.0                   # the back's groove that sits on it
STRAP_W, STRAP_T = 20.0, 2.0         # a strap round the handle, through two tunnels
TUNNEL_YS = (H * 0.27, H * 0.73)
TUNNEL_Z = (SADDLE_DEPTH + 1.2, SADDLE_DEPTH + 1.2 + 2.6)


def rounded_rect(w, h, r, cx=0.0, cy=0.0):
    return CrossSection.square((w - 2 * r, h - 2 * r), center=True).offset(r, join_type=_round(), circular_segments=SEG).translate((cx, cy))


def _round():
    from manifold3d import JoinType
    return JoinType.Round


def body():
    plan = rounded_rect(W, H, R, W / 2, H / 2)
    lower = plan.extrude(T - EDGE)
    top = rounded_rect(W - 2 * EDGE, H - 2 * EDGE, R - EDGE, W / 2, H / 2).extrude(0.01).translate((0, 0, T - 0.01))
    shell = Manifold.batch_hull([lower, top])
    # the screen window, recessed, with a 45-degree lead-in
    win = rounded_rect(*WINDOW, 2.0, W / 2, SCREEN_CY)
    pocket = Manifold.batch_hull([
        win.extrude(0.01).translate((0, 0, T - WINDOW_DEPTH)),
        win.offset(WINDOW_DEPTH, join_type=_round(), circular_segments=SEG).extrude(0.01).translate((0, 0, T)),
    ]).translate((0, 0, 0.005))
    # dished wells round the stick and the button
    wells = []
    for x, d, depth in ((STICK_X, STICK_KNOB + 7, 2.0), (BUTTON_X, BUTTON + 4, 1.6)):
        wells.append(Manifold.batch_hull([
            Manifold.cylinder(0.01, d / 2 - depth, circular_segments=SEG).translate((x, CONTROLS_Y, T - depth)),
            Manifold.cylinder(0.01, d / 2 + depth, circular_segments=SEG).translate((x, CONTROLS_Y, T)),
        ]))
    # the saddle on the back, along the handle
    saddle = Manifold.cylinder(H + 2, HANDLE_R + 0.3, circular_segments=SEG * 2).rotate((-90, 0, 0)).translate(
        (W / 2, -1, SADDLE_DEPTH - HANDLE_R - 0.3))
    # two strap tunnels, across the back, open at both sides
    tunnels = [Manifold.cube((W + 2, STRAP_W + 1, TUNNEL_Z[1] - TUNNEL_Z[0])).translate((-1, y - (STRAP_W + 1) / 2, TUNNEL_Z[0]))
               for y in TUNNEL_YS]
    # USB-C at the top end, for charging
    usb = Manifold.cube((9.5, 6, 3.6)).translate((W / 2 - 4.75, H - 4, T / 2 - 1.8))
    return shell - pocket - sum(wells[1:], wells[0]) - saddle - tunnels[0] - tunnels[1] - usb


def screen_bed():
    """The glass under the window (the image goes on top of it in the render)."""
    return Manifold.cube((WINDOW[0] + 3, WINDOW[1] + 3, 0.6)).translate(
        (W / 2 - (WINDOW[0] + 3) / 2, SCREEN_CY - (WINDOW[1] + 3) / 2, T - WINDOW_DEPTH - 0.6))


def stick():
    """The 5-way stick's cap: a short post with a dished, knurled-looking top."""
    post = Manifold.cylinder(2.0, 2.2, circular_segments=SEG).translate((STICK_X, CONTROLS_Y, T - 2.2))
    cap = Manifold.cylinder(3.2, STICK_KNOB / 2, STICK_KNOB / 2 - 0.6, circular_segments=SEG).translate((STICK_X, CONTROLS_Y, T - 0.6))
    dish = Manifold.sphere(9, circular_segments=SEG).translate((STICK_X, CONTROLS_Y, T + 2.6 + 9 - 0.5))
    ridges = [Manifold.cube((0.8, STICK_KNOB, 0.6), center=True).rotate((0, 0, a)).translate((STICK_X, CONTROLS_Y, T + 2.2))
              for a in range(0, 180, 30)]
    knob = cap - dish
    for r in ridges:
        knob = knob - r
    return post + knob


def button():
    """The timing button: a broad cap standing 1.5 mm proud of the face."""
    return Manifold.batch_hull([
        Manifold.cylinder(0.01, BUTTON / 2, circular_segments=SEG).translate((BUTTON_X, CONTROLS_Y, T - 1.6)),
        Manifold.cylinder(0.01, BUTTON / 2, circular_segments=SEG).translate((BUTTON_X, CONTROLS_Y, T + 0.8)),
        Manifold.cylinder(0.01, BUTTON / 2 - 1.0, circular_segments=SEG).translate((BUTTON_X, CONTROLS_Y, T + 1.5)),
    ])


def handle():
    return Manifold.cylinder(240, HANDLE_R, circular_segments=SEG * 2).rotate((-90, 0, 0)).translate(
        (W / 2, -90, SADDLE_DEPTH - HANDLE_R))


def strap():
    """A strap round the handle and through each tunnel."""
    cz = SADDLE_DEPTH - HANDLE_R
    def loop(grow):
        ring = CrossSection.circle(HANDLE_R + grow, circular_segments=SEG * 2).translate((W / 2, cz))
        bar = CrossSection.square((W + 2 * grow - 0.4, TUNNEL_Z[1] - TUNNEL_Z[0] - 0.6 + 2 * grow)).translate(
            (-grow + 0.2, TUNNEL_Z[0] + 0.3 - grow))
        return CrossSection.batch_hull([ring, bar])
    band = loop(STRAP_T) - loop(0.0)
    parts = []
    for y in TUNNEL_YS:
        m = band.extrude(STRAP_W).rotate((90, 0, 0)).translate((0, y + STRAP_W / 2, 0))
        parts.append(m)
    return parts[0] + parts[1]


def save(m, name):
    mesh = m.to_mesh()
    trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts)).export(OUT / f"model-{name}.stl")


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    for name, part in (("body", body()), ("glass", screen_bed()), ("stick", stick()), ("button", button()),
                       ("handle", handle()), ("strap", strap())):
        save(part, name)
        print(f"{name:7} {part.volume() / 1000:6.1f} cm3")
    print(f"case {W:.0f} x {H:.0f} x {T:.0f} mm; window {WINDOW[0]:.1f} x {WINDOW[1]:.1f}; screen centre y {SCREEN_CY:.2f}")
