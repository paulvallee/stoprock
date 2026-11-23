"""stoprock two-part TPU case for the M5Stack CoreInk: printable STL files for quoting.

    pip install manifold3d trimesh
    python hardware/case/case.py      -> hardware/case/*.stl (sizes and volumes for QUOTE.md)
    hardware/case/render/             -> preview.png (three.js in a headless browser; see its README)

Coordinates in millimetres: X across the width, Y up the height (Y = 0 at the bottom, where
USB-C is), Z through the thickness (Z = 0 the back).

The CoreInk's dimensions and the positions of its buttons, dial and ports come from our own record,
hardware/coreink/coreink.json (see hardware/coreink/DIMENSIONS.md).
"""
import json
from pathlib import Path

import numpy as np
import trimesh
from manifold3d import CrossSection, JoinType, Manifold, OpType

HERE = Path(__file__).resolve().parent
CI = json.loads((HERE.parent / "coreink" / "coreink.json").read_text())   # the CoreInk's dimensions
SEG = 64  # segments per full circle on rounded corners

# ------------------------------------------------------------------ dimensions (mm)
DEV_W, DEV_H, DEV_T = CI["body"]["width"], CI["body"]["height"], CI["body"]["depth"]
DEV_R = 3.0                              # its corner radius (MEASURE)
FIT = 0.2                                # clearance round the device, each side
WALL = 2.5                               # side walls
FLOOR = 2.5                              # back
BEZEL_T = 2.5                            # front frame
CW, CH = DEV_W + 2 * WALL, DEV_H + 2 * WALL
CT = FLOOR + DEV_T + BEZEL_T
CASE_R = 6.0
CAV_W, CAV_H = DEV_W + 2 * FIT, DEV_H + 2 * FIT
DEV_Z0, DEV_Z1 = FLOOR, FLOOR + DEV_T    # the device's back and front faces
MID_Z = FLOOR + DEV_T / 2                # middle of the device's thickness
DEV_X0, DEV_TOP = WALL, WALL + DEV_H     # the device's left edge and top, in case coordinates


def dev(x=None, y=None, z=None):
    """Case coordinates of a point given in the CoreInk's own coordinates (coreink.json: x from its
    left edge, y up from its bottom, z from its back face)."""
    out = []
    if x is not None: out.append(x + WALL)
    if y is not None: out.append(y + WALL)
    if z is not None: out.append(z + FLOOR)
    return out[0] if len(out) == 1 else tuple(out)


def mid(r):
    return (r[0] + r[1]) / 2

RABBET = 1.2         # the tub's rim is cut back this much inside, for the bezel to sit in
GROOVE = 0.4         # snap groove in the rim (the bezel's bead clicks into it)
GROOVE_Z = (DEV_Z1 + 0.9, DEV_Z1 + 1.7)
BEAD = 0.35          # the bezel's bead, a little smaller than the groove

WEB = 0.8            # flexible web each button pad sits on
MOAT = 1.0           # gap round a pad that frees it from the wall
PROUD = 1.5          # how far pads stand out
NUB = 0.15           # how far a pad's inner nub reaches into the 0.2 mm fit gap (tune by test print)

# Openings, from coreink.json: centres and sizes, in case coordinates
_f, _t, _r, _b, _k = CI["front"], CI["top"], CI["right"], CI["bottom"], CI["back"]
SCREEN = 30.0                                          # window (the CoreInk's is 28.75 x 28.25)
SCREEN_CY = dev(y=mid(_f["screen_window"]["y"]))

TOP_BTN = dict(cx=dev(x=mid(_t["button"]["x"])), cz=dev(z=mid(_t["button"]["z"])), w=12.0, t=7.0)
SIDE_BTN = dict(cy=dev(y=mid(_r["power_button"]["y"])), cz=dev(z=mid(_r["power_button"]["z"])), h=6.5, t=10.0)
# Rocker (dial) cut-out, right side: the CoreInk's dial window plus 0.5 mm at each end and 0.25 mm
# at the back edge (it runs through to the front face), with a chamfer round the outside so a thumb
# reaches the dial through the 2.5 mm wall.
_dial = _r["dial_window"]
ROCKER = dict(cy=dev(y=mid(_dial["y"])), cz=dev(z=(_dial["z"][0] - 0.25 + _dial["z"][1] + 0.25) / 2),
              h=_dial["y"][1] - _dial["y"][0] + 1.0, t=_dial["z"][1] - _dial["z"][0] + 0.5, chamfer=1.2)
_port = _b["port_opening"]
USB = dict(cx=dev(x=mid(_port["x"])), cz=dev(z=mid(_port["z"])), w=12.5, t=10.0)   # bottom: 12.25 x 9.75
RESET = (dev(x=mid(_k["reset_button"]["x"])), dev(y=mid(_k["reset_button"]["y"])))
LOOP = None  # the old corner loop (dict(c=(-1.5, CH + 1.5), r_out=5.75, r_in=2.5, t=3.5)); replaced by the mount

# Back mount, for a lanyard or a broom: the back is thicker and has one tunnel running across it,
# open at both sides, in the upper third. A lanyard cord through it hangs the case upright and
# centred; a silicone band through it (its bonded overlap inside) holds the case on a broom handle.
# The back stays flat, so the tub prints standing on it (two rails down the back were dropped: the
# tub would have stood on them).
MOUNT_BACK = 2.0                 # how much thicker the back is (outer back at z = -MOUNT_BACK)
TUNNEL = dict(cy=DEV_TOP - 16.0, w=21.0, z0=-0.6, z1=1.4)   # across X; 21 x 2 mm opening
RAILS = None   # was dict(dx=10.5, w=3.0, h=1.2, y0=8.0, y1=CH - 8.0): rails down the back

# The stoprock wordmark debossed in the back, below the tunnel, mirrored so it reads from behind
LOGO_BACK = dict(w=34.0, cy=17.0, depth=0.5)


# ------------------------------------------------------------------ shape helpers
def rrect(w, h, r):
    r = min(r, w / 2 - 1e-3, h / 2 - 1e-3)
    return CrossSection.square((w - 2 * r, h - 2 * r), center=True).offset(r, JoinType.Round, circular_segments=SEG)


def prism_z(cx, cy, w, h, r, z0, z1):
    """Rounded rectangle w x h (X x Y) centred on (cx, cy), from z0 to z1."""
    return Manifold.extrude(rrect(w, h, r), z1 - z0).translate((cx, cy, z0))


def prism_x(cy, cz, h, t, r, x0, x1):
    """Rounded rectangle h x t (Y x Z) centred on (cy, cz), from x0 to x1."""
    return Manifold.extrude(rrect(t, h, r), x1 - x0).rotate((0, 90, 0)).translate((x0, cy, cz))


def prism_y(cx, cz, w, t, r, y0, y1):
    """Rounded rectangle w x t (X x Z) centred on (cx, cz), from y0 to y1."""
    return Manifold.extrude(rrect(w, t, r), y1 - y0).rotate((-90, 0, 0)).translate((cx, y0, cz))


def ring_z(cx, cy, r_out, r_in, z0, z1):
    outer = Manifold.cylinder(z1 - z0, r_out, circular_segments=SEG).translate((cx, cy, z0))
    inner = Manifold.cylinder(z1 - z0 + 2, r_in, circular_segments=SEG).translate((cx, cy, z0 - 1))
    return outer - inner


def pad(prism, outward, footprint, inner_face, sign):
    """A button pad on a wall: frees a footprint of the wall with a moat (leaving a WEB on the
    inside), stands it PROUD of the outside, and adds a nub on the inside.
    prism(a0, a1, grow) -> the footprint grown by `grow`, between a0 and a1 along the wall's normal.
    sign: +1 if the outside is towards larger coordinates."""
    web_out = inner_face + sign * WEB                    # where the moat stops
    outside = inner_face + sign * WALL - sign * FIT      # the wall's outer face
    lo, hi = sorted((web_out, outside + sign * 0.01))
    moat = prism(lo, hi, MOAT) - prism(lo - 0.1, hi + 0.1, 0)
    a0, a1 = sorted((outside - sign * 0.01, outside + sign * PROUD))
    proud = prism(a0, a1, 0)
    n0, n1 = sorted((inner_face, inner_face - sign * NUB))
    nub = footprint(n0, n1)
    return moat, proud + nub


# ------------------------------------------------------------------ the logo
def logo_solid(width):
    """The stoprock wordmark as a solid LOGO_BACK depth thick, centred on the origin in X and Y,
    from z = 0 up, mirrored in X so it reads correctly from the back. Built from the logo image
    (tools/mockups/logo.py) one row of pixels at a time, then smoothed a little."""
    import sys
    sys.path.insert(0, str(HERE.parents[1] / "tools" / "mockups"))
    import logo
    from PIL import Image
    px = 0.08                                    # mm per pixel
    wm = logo.wordmark(band=0.14).convert("L")
    n = int(width / px)
    img = wm.resize((n, round(wm.height * n / wm.width)), Image.LANCZOS)
    w, h = img.size
    data = img.load()
    rects = []
    for y in range(h):
        x = 0
        while x < w:
            if data[x, y] < 128:
                x0 = x
                while x < w and data[x, y] < 128:
                    x += 1
                # mirrored: image x grows to the right, the model's x to the left as seen from behind
                rects.append(CrossSection.square(((x - x0) * px, px * 1.02)).translate(((w - x) * px - width / 2, (h - 1 - y) * px - h * px / 2)))
            else:
                x += 1
    shape = CrossSection.batch_boolean(rects, OpType.Add).offset(0.03, JoinType.Round).offset(-0.03, JoinType.Round)
    return Manifold.extrude(shape, LOGO_BACK["depth"] + 0.01)


# ------------------------------------------------------------------ the tub
def tub():
    body = prism_z(CW / 2, CH / 2, CW, CH, CASE_R, 0, CT)
    body -= prism_z(CW / 2, CH / 2, CAV_W, CAV_H, DEV_R + FIT, FLOOR, CT + 1)            # the device's pocket
    body -= prism_z(CW / 2, CH / 2, CAV_W + 2 * RABBET, CAV_H + 2 * RABBET, DEV_R + FIT + RABBET, DEV_Z1, CT + 1)
    g = RABBET + GROOVE
    body -= prism_z(CW / 2, CH / 2, CAV_W + 2 * g, CAV_H + 2 * g, DEV_R + FIT + g, *GROOVE_Z)

    # top button (outside towards +Y)
    tb = TOP_BTN
    face_top = CH / 2 + CAV_H / 2
    moat, add = pad(lambda a0, a1, grow: prism_y(tb["cx"], tb["cz"], tb["w"] + 2 * grow, tb["t"] + 2 * grow, 1.5 + grow, a0, a1),
                    +1, lambda a0, a1: Manifold.cylinder(a1 - a0, 1.2, circular_segments=SEG).rotate((-90, 0, 0)).translate((tb["cx"], a0, tb["cz"])),
                    face_top, +1)
    body = body - moat + add

    # side button (power), right wall (outside towards +X), with a low guard rim round it
    sb = SIDE_BTN
    face_right = CW / 2 + CAV_W / 2
    moat, add = pad(lambda a0, a1, grow: prism_x(sb["cy"], sb["cz"], sb["h"] + 2 * grow, sb["t"] + 2 * grow, 1.5 + grow, a0, a1),
                    +1, lambda a0, a1: Manifold.cylinder(a1 - a0, 1.2, circular_segments=SEG).rotate((0, 90, 0)).translate((a0, sb["cy"], sb["cz"])),
                    face_right, +1)
    body = body - moat + add
    rim_out = prism_x(sb["cy"], sb["cz"], sb["h"] + 6.4, sb["t"] + 6.4, 4.7, CW - 0.01, CW + PROUD - 0.3)
    rim_in = prism_x(sb["cy"], sb["cz"], sb["h"] + 4.4, sb["t"] + 4.4, 3.7, CW - 0.02, CW + PROUD)
    body += rim_out - rim_in

    # rocker: an open cut-out in the right wall (a soft cover is a later iteration)
    rk = ROCKER
    x_in, c = CW - WALL - 0.5, rk["chamfer"]
    body -= prism_x(rk["cy"], rk["cz"], rk["h"], rk["t"], 2.0, x_in, CW + 0.5)
    # the chamfer: a tapered cut growing by c each side over the outer c mm of the wall
    flare = Manifold.extrude(rrect(rk["t"], rk["h"], 2.0), c + 0.02,
                             scale_top=((rk["t"] + 2 * c) / rk["t"], (rk["h"] + 2 * c) / rk["h"]))
    body -= flare.rotate((0, 90, 0)).translate((CW - c - 0.01, rk["cy"], rk["cz"]))

    # USB-C opening, bottom wall
    us = USB
    body -= prism_y(us["cx"], us["cz"], us["w"], us["t"], 2.0, -0.5, WALL + 0.5)


    # lanyard loop at the top-left corner (replaced by the back mount)
    if LOOP:
        lp = LOOP
        body += ring_z(*lp["c"], lp["r_out"], lp["r_in"], MID_Z - lp["t"] / 2, MID_Z + lp["t"] / 2)

    # back mount: a thicker back with a tunnel across it, and two rails
    body += prism_z(CW / 2, CH / 2, CW, CH, CASE_R, -MOUNT_BACK, 0.01)
    tn = TUNNEL
    body -= Manifold.cube((CW + 2, tn["w"], tn["z1"] - tn["z0"])).translate((-1, tn["cy"] - tn["w"] / 2, tn["z0"]))
    if RAILS:
        rl = RAILS
        for sx in (-1, 1):
            x = CW / 2 + sx * rl["dx"]
            body += prism_z(x, (rl["y0"] + rl["y1"]) / 2, rl["w"], rl["y1"] - rl["y0"], rl["w"] / 2 - 0.01,
                            -MOUNT_BACK - rl["h"], -MOUNT_BACK + 0.01)

    # reset pinhole, through the back: skin, tunnel and floor (slide a band or cord aside to use it)
    body -= Manifold.cylinder(FLOOR + MOUNT_BACK + 1, 0.9, circular_segments=SEG).translate((RESET[0], RESET[1], -MOUNT_BACK - 0.5))

    # the wordmark, debossed
    lg = LOGO_BACK
    body -= logo_solid(lg["w"]).translate((CW / 2, lg["cy"], -MOUNT_BACK - 0.01 - 1)).translate((0, 0, 1))
    return body


# ------------------------------------------------------------------ for the renders only: a broom
# handle with a strap round it, and a lanyard (not printed)
HANDLE_R = 14.0


def capsule_path(points, r):
    """A round cord along a polyline: one capsule (hull of two spheres) per segment."""
    balls = [Manifold.sphere(r, circular_segments=24).translate(p) for p in points]
    return Manifold.batch_boolean([Manifold.batch_hull([a, b]) for a, b in zip(balls, balls[1:])], OpType.Add)


BAND_T = 1.0           # silicone band thickness
BAND_W = 20.0          # and width
CORD_R = 1.5           # 3 mm lanyard cord


def broom_band():
    """A silicone band through the tunnel and round the handle, its ends overlapped and bonded
    inside the tunnel (the doubled part)."""
    tn = TUNNEL
    back = -MOUNT_BACK - (RAILS["h"] if RAILS else 0)
    hz = -MOUNT_BACK - HANDLE_R
    z_in = tn["z0"] + 0.1                      # the band's inner face, in the tunnel
    core = CrossSection.hull_points([(0, z_in), (CW, z_in), (0, back), (CW, back)]
                                    + [(CW / 2 + HANDLE_R * np.cos(a), hz + HANDLE_R * np.sin(a))
                                       for a in np.linspace(0, 2 * np.pi, 96, endpoint=False)])
    ring = core.offset(BAND_T, JoinType.Round, circular_segments=64) - core
    lap = CrossSection.square((CW - 12, BAND_T - 0.05)).translate((6, z_in + BAND_T))   # the overlap, on top
    band2d = ring + lap
    return Manifold.extrude(band2d, BAND_W).rotate((90, 0, 0)).translate((0, tn["cy"] + BAND_W / 2, 0))


def cord_lanyard():
    """A 3 mm cord through the tunnel; its ends rise up the sides to a sliding cord-lock bead,
    then go on up as the neck loop."""
    tn = TUNNEL
    z = (tn["z0"] + tn["z1"]) / 2
    y, bead_y = tn["cy"], tn["cy"] + 62
    pts_l = [(CW / 2, y, z), (-0.5, y, z), (-3.0, y + 5, z), (CW / 2 - 2.2, bead_y - 6, z), (CW / 2 - 2.2, bead_y + 6, z),
             (-25, bead_y + 120, z)]
    pts_r = [(CW / 2, y, z), (CW + 0.5, y, z), (CW + 3.0, y + 5, z), (CW / 2 + 2.2, bead_y - 6, z), (CW / 2 + 2.2, bead_y + 6, z),
             (CW + 25, bead_y + 120, z)]
    cord = capsule_path(pts_l, CORD_R) + capsule_path(pts_r, CORD_R)
    bead = Manifold.cylinder(11, 5.0, circular_segments=48).rotate((-90, 0, 0)).translate((CW / 2, bead_y - 5.5, z))
    button = Manifold.cylinder(3, 2.4, circular_segments=32).translate((CW / 2, bead_y, z + 4.0))
    return cord, bead + button


def scene_parts():
    tn = TUNNEL
    hz = -MOUNT_BACK - HANDLE_R                  # the handle touches the back between the rails
    handle = Manifold.cylinder(260, HANDLE_R, circular_segments=96).rotate((-90, 0, 0)).translate((CW / 2, -120, hz))
    cord, cordlock = cord_lanyard()
    return {"handle": handle, "band": broom_band(), "cord": cord, "cordlock": cordlock}


# ------------------------------------------------------------------ the bezel
# The front is faceted: the outer edge is flush with the tub, and the window's corners rise to
# different heights, so the frame is made of tilted triangular planes (a cut-gem look that also
# stands the frame proud of the screen). Heights in mm above the tub's front, from the top-left
# window corner, clockwise.
FACET_RISE = (2.6, 0.8, 2.2, 0.6)
FACET_INNER_MID_RISE = (1.2, 2.0, 0.9, 1.7)  # the middle of each window side: top, right, bottom, left
FACET_MID_RISE = 0.0        # the middle of each outer side (0: the outer edge stays flush)
FACET_CHAMFER = 4.0         # the outer outline cuts the plate's rounded corners at 45 degrees


def facets(w, h):
    """The faceted top: convex pieces (one per triangle, down to the plate's front face) unioned."""
    x0, x1 = CW / 2 - w / 2, CW / 2 + w / 2
    y0, y1 = CH / 2 - h / 2, CH / 2 + h / 2
    c = FACET_CHAMFER
    # window corners, clockwise from top-left
    wx0, wx1 = CW / 2 - SCREEN / 2, CW / 2 + SCREEN / 2
    wy0, wy1 = SCREEN_CY - SCREEN / 2, SCREEN_CY + SCREEN / 2
    inner = [(wx0, wy1), (wx1, wy1), (wx1, wy0), (wx0, wy0)]
    inner = [(x, y, CT + r) for (x, y), r in zip(inner, FACET_RISE)]
    # outer outline, clockwise from the top-left corner: each corner is cut by a chamfer (two points)
    corners = [((x0, y1 - c), (x0 + c, y1)), ((x1 - c, y1), (x1, y1 - c)),
               ((x1, y0 + c), (x1 - c, y0)), ((x0 + c, y0), (x0, y0 + c))]
    tris = []
    for k in range(4):
        a, b = inner[k], inner[(k + 1) % 4]
        m = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, CT + FACET_INNER_MID_RISE[k])   # window side's middle
        A2 = (*corners[k][1], CT)               # end of this corner's chamfer
        B1 = (*corners[(k + 1) % 4][0], CT)     # start of the next corner's chamfer
        M = ((A2[0] + B1[0]) / 2, (A2[1] + B1[1]) / 2, CT + FACET_MID_RISE)
        A1 = (*corners[k][0], CT)
        tris += [(A1, A2, a), (A2, M, a), (M, m, a), (M, b, m), (M, B1, b)]
    pieces = []
    for t in tris:
        pts = [p for p in t] + [(x, y, CT - 0.3) for x, y, _ in t]
        pieces.append(Manifold.hull_points(pts))
    return Manifold.batch_boolean(pieces, OpType.Add)


def bezel():
    w, h = CAV_W + 2 * RABBET - 0.2, CAV_H + 2 * RABBET - 0.2      # sits in the rim's rabbet
    r = DEV_R + FIT + RABBET - 0.1
    plate = prism_z(CW / 2, CH / 2, w, h, r, DEV_Z1, CT)
    bead_w, bead_h = w + 2 * BEAD, h + 2 * BEAD
    plate += prism_z(CW / 2, CH / 2, bead_w, bead_h, r + BEAD, GROOVE_Z[0] + 0.05, GROOVE_Z[1] - 0.05)
    plate += facets(w, h)
    plate -= prism_z(CW / 2, SCREEN_CY, SCREEN, SCREEN, 1.5, DEV_Z1 - 1, CT + 5)             # screen window
    # no LED window: the green LED isn't used (the stopwatch icon shows timing in progress)
    return plate


# ------------------------------------------------------------------ export
def to_trimesh(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(np.asarray(mesh.vert_properties)[:, :3], np.asarray(mesh.tri_verts), process=False)  # merging near-duplicate points can break the closed mesh


def main():
    t, b = tub(), bezel()
    parts = {"tub": t, "bezel": b}
    stats = {}
    for name, m in parts.items():
        tm = to_trimesh(m)
        assert tm.is_watertight, name
        printable = tm.copy()
        # both print as modelled: the tub on its back, the bezel on its flat back (faceted front up)
        tm.export(HERE / "render" / f"model-{name}.stl")     # in model coordinates, for the renders
        printable.apply_translation(-printable.bounds[0])
        out = HERE / f"stoprock-case-{name}.stl"
        printable.export(out)
        ext = printable.extents
        stats[name] = (ext, tm.volume / 1000)
        print(f"wrote {out.relative_to(HERE.parents[1])}: {ext[0]:.1f} x {ext[1]:.1f} x {ext[2]:.1f} mm, "
              f"{tm.volume / 1000:.1f} cm3, about {tm.volume / 1000 * 1.21:.0f} g of TPU")
    for name, m in scene_parts().items():
        to_trimesh(m).export(HERE / "render" / f"model-{name}.stl")
    return stats


if __name__ == "__main__":
    main()
