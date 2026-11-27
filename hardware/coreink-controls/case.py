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
BACK = 6.5            # the back under the CoreInk's wiring: the saddle, and the strap tunnels under the wiring
WIRING = 5.5          # room behind the CoreInk for the right-angle header and its plugs
LIP = 1.5             # the front frame over the CoreInk's face
EDGE = 2.0            # the front edge's rounding

W = DEV_W + 2 * (FIT + WALL)
T = BACK + WIRING + DEV_T + LIP
DEV_X0 = WALL + FIT                    # the CoreInk's left edge (as the case is seen)
DEV_Z0 = BACK + WIRING                 # its back face

# ------------------------------------------------------------------ the tail: the stick above the button
TAIL_W, TAIL_T = 30.0, 18.5            # as wide as the handle (about 29 mm); thinner than the body
SKIN = 1.6                             # the tail's front wall
STICK_KNOB, BUTTON = 13.0, 16.0        # the stick's cap: PS2-style, sized to the switch's short travel
# The switch registers a tilt of about 0.25 mm at 4.3 mm up its stem (about 3 degrees): at the 13 mm cap's
# rim that is about 0.4 mm. A low collar round the cap, 1 mm clear of it, guards its edge from a glove and
# stops it tilting much further than that.
COLLAR = dict(inner=STICK_KNOB / 2 + 1.0, outer=STICK_KNOB / 2 + 3.2, height=1.6)
CAP_LIFT = 1.0                         # the cap's underside above the face, inside the collar
RAMP = 9.0                             # the face slopes down from the body to the tail over this length
STICK_Y = 13.0 + 10.0 + 2.0 + 8.0      # from the tail's bottom: margin, button well, gap, stick well
BUTTON_Y = 13.0
SHOULDER = STICK_Y + 8.0 + 3.0 + RAMP  # where the body (full thickness) begins
DEV_Y0 = SHOULDER + WALL + FIT         # the CoreInk's bottom (its top edge, it being turned over)
H = DEV_Y0 + DEV_H + FIT + WALL

HANDLE_R, SADDLE_DEPTH = 14.3, 3.0     # 1 1/8" handle (28.6 mm)
STRAP_W = 16.0
TUNNEL_Z = (SADDLE_DEPTH + 0.4, SADDLE_DEPTH + 0.4 + 2.2)
TUNNEL_YS = (DEV_Y0 + 11.0, DEV_Y0 + 49.0)


# ------------------------------------------------------------------ the parts inside
# The right-angle header in the CoreInk's back socket (window x 3.5-8.75, y 8.75-29.75 in its own
# coordinates): its legs in the socket's two columns, its body beside the window, the bent pins and the
# jumper plugs on them lying across the back toward the middle. Socket pin n: column n odd/even, position
# (n - 1) // 2 along the socket; the 8 pins not used are snipped off.
SOCKET_COLS = (4.855, 7.395)
HEADER_BODY = (9.0, 11.5)              # its x span, CoreInk coordinates
PLUG_LEN = 14.0
ROW_Z = {1: -3.81, 0: -1.27}           # below the CoreInk's back: odd pins in the far row, even in the near
PINS = {1: "gnd", 6: "vcc", 7: "up", 8: "down", 14: "left", 12: "right", 13: "centre", 16: "time"}
# PMD Way's 5-way breakout (SparkFun's design): about 20.3 x 19 x 1.6, the switch in the middle, a row of 7
# pads (+ U C L D R -) along one edge. It is mounted turned round, the pads toward the CoreInk; the firmware
# maps the directions back. Wires are soldered to the pads from behind.
PMD_W, PMD_H, PMD_T = 20.3, 19.0, 1.6
PMD_PADS = ("-", "R", "D", "L", "C", "U", "+")     # left to right, as seen from the front, turned round
PMD_Z = BACK + 2.0                     # the board stands on posts, the wires under it
SWITCH = (7.5, 3.5)                    # the 5-way switch body's side and height
STEM = (2.6, 3.5)
# Adafruit 1119: a 12 mm tactile switch, 6 mm tall with its actuator; its legs at the corners.
TACT = (12.0, 3.8, 2.2)                # body side, body height, actuator height
TACT_Z = TAIL_T - SKIN - TACT[1] - TACT[2]           # on a shelf; its actuator meets the cap


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
    for y, d, depth in ((BUTTON_Y, BUTTON + 4, 1.6),):
        wells.append(Manifold.batch_hull([
            Manifold.cylinder(0.01, d / 2 - depth, circular_segments=SEG).translate((W / 2, y, TAIL_T - depth)),
            Manifold.cylinder(0.01, d / 2 + depth, circular_segments=SEG).translate((W / 2, y, TAIL_T + 0.01)),
        ]))
    saddle = Manifold.cylinder(H + 2, HANDLE_R + 0.3, circular_segments=SEG * 2).rotate((-90, 0, 0)).translate(
        (W / 2, -1, SADDLE_DEPTH - HANDLE_R - 0.3))
    tunnels = [Manifold.cube((W + 2, STRAP_W + 1, TUNNEL_Z[1] - TUNNEL_Z[0])).translate((-1, y - (STRAP_W + 1) / 2, TUNNEL_Z[0]))
               for y in TUNNEL_YS]
    cut = window + port + port_bevel + dial + power + wells[0] + saddle + tunnels[0] + tunnels[1]
    # the collar round the stick: a low ring on the tail's face, its top rounded
    ring = Manifold.batch_hull([
        Manifold.cylinder(0.01, COLLAR["outer"], circular_segments=SEG).translate((W / 2, STICK_Y, TAIL_T - 0.5)),
        Manifold.cylinder(0.01, COLLAR["outer"] - 0.8, circular_segments=SEG).translate((W / 2, STICK_Y, TAIL_T + COLLAR["height"])),
    ]) - Manifold.cylinder(TAIL_T, COLLAR["inner"], circular_segments=SEG).translate((W / 2, STICK_Y, TAIL_T - 1.0))
    return shell + ring - cut - inside() + mounts()


def header_box():
    """The pocket behind the CoreInk for the header and its plugs, in case coordinates (x0, x1, y0, y1)."""
    ys = [DEV_Y0 + DEV_H - pin_y(n) for n in PINS]
    x0 = DEV_X0 + DEV_W - (HEADER_BODY[1] + 0.6 + PLUG_LEN) - 1.0
    x1 = DEV_X0 + DEV_W - SOCKET_COLS[0] + 1.5
    return x0, x1, min(ys) - 2.5, max(ys) + 2.5


def pin_y(n):
    return 19.25 + ((n - 1) // 2 - 3.5) * 2.54


def inside():
    """What is hollow: the CoreInk's pocket, the header's pocket behind it, the conduit down through the
    shoulder, and the tail's inside."""
    pocket = Manifold.cube((DEV_W + 2 * FIT, DEV_H + 2 * FIT, DEV_T + 0.01)).translate((DEV_X0 - FIT, DEV_Y0 - FIT, DEV_Z0))
    x0, x1, y0, y1 = header_box()
    header = Manifold.cube((x1 - x0, y1 - y0, WIRING + 0.01)).translate((x0, y0, BACK))
    conduit_top = y0 + 4.0
    conduit = Manifold.cube((10.0, conduit_top - (SHOULDER - RAMP - 2.0), WIRING + 0.01)).translate(
        (W / 2 - 7.0, SHOULDER - RAMP - 2.0, BACK))
    # the conduit's turn into the header pocket
    turn = Manifold.cube((W / 2 + 3.0 - x0, 6.0, WIRING + 0.01)).translate((x0, y0, BACK))
    tail = (rounded_rect(TAIL_W - 4.0, SHOULDER - RAMP, 8.0, W / 2, (SHOULDER - RAMP) / 2 + 2.0)
            .extrude(TAIL_T - SKIN - BACK).translate((0, 0, BACK)))
    holes = (Manifold.cylinder(TAIL_T, 6.2, circular_segments=SEG).translate((W / 2, STICK_Y, BACK))
             + Manifold.cylinder(TAIL_T, BUTTON / 2 + 0.4, circular_segments=SEG).translate((W / 2, BUTTON_Y, BACK)))
    return pocket + header + conduit + turn + tail + holes


def mounts():
    """Four posts with pegs for the 5-way board's corner holes, and a shelf under the tactile switch."""
    m = None
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = W / 2 + sx * (PMD_W / 2 - 2.6), STICK_Y + sy * (PMD_H / 2 - 2.6)
            post = (Manifold.cylinder(PMD_Z - BACK, 2.2, circular_segments=SEG).translate((x, y, BACK - 0.01))
                    + Manifold.cylinder(PMD_T + 0.6, 1.1, circular_segments=SEG).translate((x, y, PMD_Z - 0.01)))
            m = post if m is None else m + post
    shelf = Manifold.cube((TACT[0] - 1.0, TACT[0] - 5.0, TACT_Z - BACK + 0.01)).translate(
        (W / 2 - (TACT[0] - 1.0) / 2, BUTTON_Y - (TACT[0] - 5.0) / 2, BACK - 0.01))
    return m + shelf


def stick():
    """The PS2-style cap on the 5-way switch's stem: a shaft down through the hole onto the stem, a 13 mm
    head with a dished top and a ribbed grip ring round its rim."""
    x, y = W / 2, STICK_Y
    stem_top = PMD_Z + PMD_T + SWITCH[1] + STEM[1]
    base = TAIL_T + CAP_LIFT                    # the head's underside
    shaft = (Manifold.cylinder(base - (stem_top - 2.5) + 0.5, 3.6, circular_segments=SEG).translate((x, y, stem_top - 2.5))
             - Manifold.cube((STEM[0] + 0.2, STEM[0] + 0.2, 2.6), center=True).translate((x, y, stem_top - 1.2)))
    r, h = STICK_KNOB / 2, 4.2
    # the head: a slightly flared drum with a rounded rim (a hull of rings)
    head = Manifold.batch_hull([
        Manifold.cylinder(0.01, r - 0.8, circular_segments=SEG).translate((x, y, base)),
        Manifold.cylinder(0.01, r, circular_segments=SEG).translate((x, y, base + 1.4)),
        Manifold.cylinder(0.01, r, circular_segments=SEG).translate((x, y, base + h - 0.9)),
        Manifold.cylinder(0.01, r - 0.7, circular_segments=SEG).translate((x, y, base + h)),
    ])
    # the dish: a shallow spherical hollow in the top
    dish_r = 14.0
    head = head - Manifold.sphere(dish_r, circular_segments=SEG * 2).translate((x, y, base + h + dish_r - 1.5))
    # the grip ring: small ribs round the rim's side, like a PS2 cap's knurled edge
    for i in range(36):
        a = i * 10
        rib = Manifold.cube((0.6, 0.9, h - 2.0), center=True).translate((r, 0, 0)).rotate((0, 0, a)).translate((x, y, base + h / 2 + 0.2))
        head = head - rib
    # a texture ring of tiny pits on the dish's flat
    for i in range(18):
        a = i * 20
        pit = Manifold.sphere(0.45, circular_segments=12).translate((r - 2.0, 0, 0)).rotate((0, 0, a)).translate((x, y, base + h - 0.85))
        head = head - pit
    return shaft + head


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


def inside_parts():
    """Positions of the parts inside, for the render (case coordinates)."""
    dx = lambda x: DEV_X0 + DEV_W - x   # noqa: E731  (the CoreInk is turned over)
    pins = []
    for n, what in PINS.items():
        row = n % 2
        pins.append(dict(n=n, what=what, y=DEV_Y0 + DEV_H - pin_y(n), z=DEV_Z0 + ROW_Z[row],
                         socket_x=dx(SOCKET_COLS[0] if row else SOCKET_COLS[1])))
    pads = {name: W / 2 + (i - 3) * 2.54 for i, name in enumerate(PMD_PADS)}
    pad_y = STICK_Y + PMD_H / 2 - 1.6
    ends = {"gnd": (pads["-"], pad_y), "vcc": (pads["+"], pad_y), "up": (pads["U"], pad_y), "down": (pads["D"], pad_y),
            "left": (pads["L"], pad_y), "right": (pads["R"], pad_y), "centre": (pads["C"], pad_y),
            "time": (W / 2 + TACT[0] / 2 + 0.3, BUTTON_Y + 2.5)}
    return dict(
        header=dict(body_x=[dx(HEADER_BODY[1]), dx(HEADER_BODY[0])], plug_x=[dx(HEADER_BODY[1] + 0.6 + PLUG_LEN), dx(HEADER_BODY[1] + 0.6)],
                    z_back=DEV_Z0, pins=pins),
        ends={k: [x, y] for k, (x, y) in ends.items()},
        conduit_x=W / 2 - 2.0, wire_z=BACK + 1.0,
        pmd=dict(x=W / 2, y=STICK_Y, z=PMD_Z, w=PMD_W, h=PMD_H, t=PMD_T, switch=list(SWITCH), stem=list(STEM)),
        tact=dict(x=W / 2, y=BUTTON_Y, z=TACT_Z, size=list(TACT), gnd_leg=[W / 2 - TACT[0] / 2 - 0.3, BUTTON_Y + 2.5]),
        shoulder=SHOULDER, ramp=RAMP)


def save(m, name):
    mesh = m.to_mesh()
    trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts)).export(OUT / f"model-case-{name}.stl")


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    b = body()
    half = Manifold.cube((W / 2 + 0.3, H + 10, T + 10)).translate((-1, -5, -5))   # the left half, cut through the conduit
    for name, part in (("body", b), ("cut", b ^ half), ("stick", stick()), ("button", button()), ("handle", handle()),
                       ("strap", strap())):
        save(part, name)
    sx, sy = dev(*CI["front"]["screen_window_centre"])
    (OUT / "case.json").write_text(json.dumps(dict(
        W=W, H=H, T=T, screen=[sx, sy, DEV_Z0 + DEV_T + 0.05],
        # the CoreInk model (its own coordinates, offset 2.5 in M5Stack's file) turned over and placed
        device=dict(x0=DEV_X0, y0=DEV_Y0, z0=DEV_Z0, w=DEV_W, h=DEV_H),
        tail=dict(stick_y=STICK_Y, button_y=BUTTON_Y, t=TAIL_T),
        inside=inside_parts()), indent=1))
    print(f"case {W:.1f} x {H:.1f} x {T:.1f} mm (tail {TAIL_W:.0f} wide, {TAIL_T:.0f} thick)")
