"""stoprock two-part TPU case for the M5Stack CoreInk: printable STL files for quoting.

    pip install manifold3d trimesh
    python hardware/case/case.py      -> hardware/case/*.stl (sizes and volumes for QUOTE.md)
    hardware/case/render/             -> preview.png (three.js in a headless browser; see its README)

Coordinates in millimetres: X across the width, Y up the height (Y = 0 at the bottom, where
USB-C is), Z through the thickness (Z = 0 the back).

Positions come from M5Stack's CoreInk dimension drawing (40 x 56 x 16 mm). Where a feature sits
through the thickness, and a few positions read off the drawing by scale, are marked "MEASURE":
check them on the device before printing for use.
"""
from pathlib import Path

import numpy as np
import trimesh
from manifold3d import CrossSection, JoinType, Manifold, OpType

HERE = Path(__file__).resolve().parent
SEG = 64  # segments per full circle on rounded corners

# ------------------------------------------------------------------ dimensions (mm)
DEV_W, DEV_H, DEV_T = 40.0, 56.0, 16.0   # CoreInk, per M5Stack's drawing
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
MID_Z = FLOOR + DEV_T / 2                # middle of the device's thickness: buttons, ports (MEASURE)
DEV_X0, DEV_TOP = WALL, WALL + DEV_H     # the device's left edge and top, in case coordinates

RABBET = 1.2         # the tub's rim is cut back this much inside, for the bezel to sit in
GROOVE = 0.4         # snap groove in the rim (the bezel's bead clicks into it)
GROOVE_Z = (DEV_Z1 + 0.9, DEV_Z1 + 1.7)
BEAD = 0.35          # the bezel's bead, a little smaller than the groove

WEB = 0.8            # flexible web each button pad sits on
MOAT = 1.0           # gap round a pad that frees it from the wall
PROUD = 1.5          # how far pads stand out
NUB = 0.15           # how far a pad's inner nub reaches into the 0.2 mm fit gap (tune by test print)

# From the drawing (front view: the screen's frame about 29 mm square, centred across, its centre
# 25 mm below the top; the visible area is 27.6 mm square)
SCREEN = 30.0
SCREEN_CY = DEV_TOP - 25.0
LED = (CW - 7.0, CH - 5.0)                             # green LED (MEASURE: not on the drawing)

TOP_BTN = dict(cx=DEV_X0 + 26.0, w=12.0, t=7.0)        # top button: 22.3-29.7 mm from the left
SIDE_BTN = dict(cy=WALL + 15.0, h=10.0, t=7.0)         # right side, 15 mm above the bottom: power (MEASURE: confirm)
ROCKER = dict(cy=DEV_TOP - 20.0, h=20.0, t=9.0)        # right side, about 10-30 mm below the top: open cut-out
USB = dict(cx=DEV_X0 + 21.0, w=12.5, t=7.0)            # bottom, about centred (MEASURE its depth)
RESET = (DEV_X0 + 6.0, DEV_TOP - 15.0)                  # back: 6 mm from the left edge, 15 mm below the top
LOOP = dict(c=(-1.5, CH + 1.5), r_out=5.75, r_in=2.5, t=3.5)   # lanyard loop, top-left corner


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
    moat, add = pad(lambda a0, a1, grow: prism_y(tb["cx"], MID_Z, tb["w"] + 2 * grow, tb["t"] + 2 * grow, 1.5 + grow, a0, a1),
                    +1, lambda a0, a1: Manifold.cylinder(a1 - a0, 1.2, circular_segments=SEG).rotate((-90, 0, 0)).translate((tb["cx"], a0, MID_Z)),
                    face_top, +1)
    body = body - moat + add

    # side button (power), right wall (outside towards +X), with a low guard rim round it
    sb = SIDE_BTN
    face_right = CW / 2 + CAV_W / 2
    moat, add = pad(lambda a0, a1, grow: prism_x(sb["cy"], MID_Z, sb["h"] + 2 * grow, sb["t"] + 2 * grow, 1.5 + grow, a0, a1),
                    +1, lambda a0, a1: Manifold.cylinder(a1 - a0, 1.2, circular_segments=SEG).rotate((0, 90, 0)).translate((a0, sb["cy"], MID_Z)),
                    face_right, +1)
    body = body - moat + add
    rim_out = prism_x(sb["cy"], MID_Z, sb["h"] + 6.4, sb["t"] + 6.4, 4.7, CW - 0.01, CW + PROUD - 0.3)
    rim_in = prism_x(sb["cy"], MID_Z, sb["h"] + 4.4, sb["t"] + 4.4, 3.7, CW - 0.02, CW + PROUD)
    body += rim_out - rim_in

    # rocker: an open cut-out in the right wall (a soft cover is a later iteration)
    rk = ROCKER
    body -= prism_x(rk["cy"], MID_Z, rk["h"], rk["t"], 2.0, CW - WALL - 0.5, CW + 0.5)

    # USB-C opening, bottom wall
    us = USB
    body -= prism_y(us["cx"], MID_Z, us["w"], us["t"], 2.0, -0.5, WALL + 0.5)

    # reset pinhole, back
    body -= Manifold.cylinder(FLOOR + 1, 0.9, circular_segments=SEG).translate((RESET[0], RESET[1], -0.5))

    # lanyard loop at the top-left corner
    lp = LOOP
    body += ring_z(*lp["c"], lp["r_out"], lp["r_in"], MID_Z - lp["t"] / 2, MID_Z + lp["t"] / 2)
    return body


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
    plate -= Manifold.cylinder(CT - DEV_Z1 - 0.6, 1.5, circular_segments=SEG).translate((LED[0], LED[1], DEV_Z1 - 0.01))  # LED: 0.6 mm skin
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
        printable.apply_translation(-printable.bounds[0])
        out = HERE / f"stoprock-case-{name}.stl"
        printable.export(out)
        ext = printable.extents
        stats[name] = (ext, tm.volume / 1000)
        print(f"wrote {out.relative_to(HERE.parents[1])}: {ext[0]:.1f} x {ext[1]:.1f} x {ext[2]:.1f} mm, "
              f"{tm.volume / 1000:.1f} cm3, about {tm.volume / 1000 * 1.21:.0f} g of TPU")
    return stats


if __name__ == "__main__":
    main()
