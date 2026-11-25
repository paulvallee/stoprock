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
GLASS_LONG, GLASS_SHORT = 71.82, 36.30    # Good Display 2.66" outline (GDEY0266T90)
ACTIVE_LONG, ACTIVE_SHORT = 60.09, 30.70  # its active area (296 x 152); taken as centred for the concept
STICK_KNOB = 10.0                    # the 5-way stick's cap (Alps SKRH-type switch underneath)
BUTTON = 16.0                        # the timing button's cap
EDGE = 2.5                           # the front edge's chamfer-round
WINDOW_DEPTH = 1.6                   # the window is recessed: a face-down drop lands on the frame
T = 16.0                             # thickness

# ------------------------------------------------------------------ layouts
# stack: portrait screen, the stick above the timing button, at the bottom (like a Rockwatcher).
# row: portrait screen, the stick and the button side by side at the bottom.
# landscape: the screen across the top, the stick and button side by side below (the first concept).
LAYOUTS = {
    "stack": dict(W=48.0, H=125.0, R=9.0, portrait=True, stick=(24.0, 36.0), button=(24.0, 15.0)),
    "row": dict(W=50.0, H=109.0, R=9.0, portrait=True, stick=(14.5, 15.0), button=(35.0, 15.0)),
    "landscape": dict(W=84.0, H=80.0, R=10.0, portrait=False, stick=(22.0, 19.0), button=(61.0, 19.0)),
    # paperdiy: M5Stack's PaperDIY kit (4.7", 540 x 960) upright, the stick above the timing button.
    # Its panel is 62.5 x 115.3 x 1.2 mm with the board (61.2 x 47.1) on the back of its lower end,
    # 5.5 mm in all; the window is the visible area M5Stack's own enclosure shows (59.6 x 105.1), its
    # top 1 mm below the panel's top edge (the panel's wide border, 9.7 mm, is at the board end).
    # Below the screen the case narrows to a tail the width of the handle for the stick and the button.
    # The kit's board sticks out 4.76 mm below the panel in two corner tabs (0-14.1 and 48-62.5 mm from
    # the panel's left edge, seen from the front) with USB-C in the left one and the power button in the
    # right, both facing down: the tail fits between them, so both face down onto open shoulders.
    "paperdiy": dict(W=69.0, H=160.0, R=8.0, T=15.0, portrait=True, stick=(34.5, 32.5), button=(34.5, 13.0),
                     glass=(62.5, 115.3), window=(59.6, 105.1), glass_margin=3.3, window_top=1.0,
                     screen="screen-paperdiy.png", broom=True, tail=30.0),
}


def configure(name):
    """Sets the module's dimensions for a layout."""
    global LAYOUT, W, H, R, T, SCREEN_CY, WINDOW, STICK_X, STICK_Y, BUTTON_X, BUTTON_Y, TUNNEL_YS, GLASS, ACTIVE
    global GLASS_TOP, CONFIG
    L = CONFIG = LAYOUTS[name]
    LAYOUT, W, H, R, T = name, L["W"], L["H"], L["R"], L.get("T", 16.0)
    if "glass" in L:
        GLASS, WINDOW = L["glass"], L["window"]
        ACTIVE = (WINDOW[0] - 1.2, WINDOW[1] - 1.2)
    else:
        GLASS = (GLASS_SHORT, GLASS_LONG) if L["portrait"] else (GLASS_LONG, GLASS_SHORT)
        ACTIVE = (ACTIVE_SHORT, ACTIVE_LONG) if L["portrait"] else (ACTIVE_LONG, ACTIVE_SHORT)
        WINDOW = (ACTIVE[0] + 1.2, ACTIVE[1] + 1.2)
    GLASS_TOP = H - L.get("glass_margin", 6.0)          # the glass's top edge
    if "window_top" in L:
        SCREEN_CY = GLASS_TOP - L["window_top"] - WINDOW[1] / 2
    else:
        SCREEN_CY = GLASS_TOP - GLASS[1] / 2
    (STICK_X, STICK_Y), (BUTTON_X, BUTTON_Y) = L["stick"], L["button"]
    TUNNEL_YS = (H * 0.27, H * 0.73)
    if L.get("tail"):  # both tunnels in the wide part, clear of the shoulders
        pb = GLASS_TOP - GLASS[1]
        TUNNEL_YS = (pb + 16.0, H - 24.0)


# ------------------------------------------------------------------ the broom
HANDLE_R = 14.5                      # about 29 mm (1 1/8")
SADDLE_DEPTH = 3.0                   # the back's groove that sits on it
STRAP_W, STRAP_T = 20.0, 2.0         # a strap round the handle, through two tunnels
TUNNEL_Z = (SADDLE_DEPTH + 1.2, SADDLE_DEPTH + 1.2 + 2.6)


def rounded_rect(w, h, r, cx=0.0, cy=0.0):
    return CrossSection.square((w - 2 * r, h - 2 * r), center=True).offset(r, join_type=_round(), circular_segments=SEG).translate((cx, cy))


def _round():
    from manifold3d import JoinType
    return JoinType.Round


# The PaperDIY's bottom edge (front view, from the panel's left edge): the board's two tabs, and the
# USB-C port and power button in them, facing down.
TAB_DROP = 4.76
TABS = ((0.0, 14.1), (48.0, 62.5))
USB_X, USB_W, USB_H = (2.0, 12.6), 8.94, 3.3     # the port's span, and the receptacle's size
POWER_X = 58.5                                   # the power button's centre
SHOULDER_WALL = 1.5                              # under the tabs


def rounded_piece(w, h, r, cx, cy):
    """A rounded-edged slab: the plan's outline, its front edge eased by EDGE."""
    lower = rounded_rect(w, h, r, cx, cy).extrude(T - EDGE)
    top = rounded_rect(w - 2 * EDGE, h - 2 * EDGE, r - EDGE, cx, cy).extrude(0.01).translate((0, 0, T - 0.01))
    return Manifold.batch_hull([lower, top])


def panel_bottom():
    return GLASS_TOP - GLASS[1]


def body():
    tail = CONFIG.get("tail")
    if not tail:
        shell = rounded_piece(W, H, R, W / 2, H / 2)
    else:
        shoulder = panel_bottom() - TAB_DROP - SHOULDER_WALL        # the wide part's bottom face
        wide = rounded_piece(W, H - shoulder, R, W / 2, (H + shoulder) / 2)
        neck = rounded_piece(tail, shoulder + R + 2, tail / 2 - 1, W / 2, (shoulder + R + 2) / 2)
        # concave fillets where the tail meets the shoulders
        plan = (rounded_rect(W, H - shoulder, R, W / 2, (H + shoulder) / 2)
                + rounded_rect(tail, shoulder + R + 2, tail / 2 - 1, W / 2, (shoulder + R + 2) / 2))
        closed = plan.offset(3.0, join_type=_round(), circular_segments=SEG).offset(-3.0, join_type=_round(), circular_segments=SEG)
        fillets = (closed - plan).extrude(T - EDGE)
        shell = wide + neck + fillets
    # the screen window, recessed, with a 45-degree lead-in
    win = rounded_rect(*WINDOW, 2.0, W / 2, SCREEN_CY)
    pocket = Manifold.batch_hull([
        win.extrude(0.01).translate((0, 0, T - WINDOW_DEPTH)),
        win.offset(WINDOW_DEPTH, join_type=_round(), circular_segments=SEG).extrude(0.01).translate((0, 0, T)),
    ]).translate((0, 0, 0.005))
    # dished wells round the stick and the button
    wells = []
    for x, y, d, depth in ((STICK_X, STICK_Y, STICK_KNOB + 6, 2.0), (BUTTON_X, BUTTON_Y, BUTTON + 4, 1.6)):
        wells.append(Manifold.batch_hull([
            Manifold.cylinder(0.01, d / 2 - depth, circular_segments=SEG).translate((x, y, T - depth)),
            Manifold.cylinder(0.01, d / 2 + depth, circular_segments=SEG).translate((x, y, T)),
        ]))
    # the saddle on the back, along the handle
    saddle = Manifold.cylinder(H + 2, HANDLE_R + 0.3, circular_segments=SEG * 2).rotate((-90, 0, 0)).translate(
        (W / 2, -1, SADDLE_DEPTH - HANDLE_R - 0.3))
    # two strap tunnels, across the back, open at both sides
    tunnels = [Manifold.cube((W + 2, STRAP_W + 1, TUNNEL_Z[1] - TUNNEL_Z[0])).translate((-1, y - (STRAP_W + 1) / 2, TUNNEL_Z[0]))
               for y in TUNNEL_YS]
    if not tail:  # USB-C at the top end, for charging
        usb = Manifold.cube((9.5, 6, 3.6)).translate((W / 2 - 4.75, H - 4, T / 2 - 1.8))
        return shell - pocket - sum(wells[1:], wells[0]) - saddle - tunnels[0] - tunnels[1] - usb
    # the kit's bottom edge faces down onto the shoulders: an opening for USB-C, a hole for the power button
    gx0 = W / 2 - GLASS[0] / 2
    port_z = T - WINDOW_DEPTH - 1.2 - 2.4                          # behind the glass, on the board
    y0 = panel_bottom() - TAB_DROP - SHOULDER_WALL - 0.5
    usb = rounded_rect(USB_W + 0.8, USB_H + 0.8, 1.6).extrude(SHOULDER_WALL + 1.0)
    usb = usb.rotate((-90, 0, 0)).translate((gx0 + sum(USB_X) / 2, y0, port_z))
    power = Manifold.cylinder(SHOULDER_WALL + 1.0, 1.9, circular_segments=SEG).rotate((-90, 0, 0)).translate((gx0 + POWER_X, y0, port_z))
    return shell - pocket - sum(wells[1:], wells[0]) - saddle - tunnels[0] - tunnels[1] - usb - power


def screen_bed():
    """The glass under the window (the image goes on top of it in the render)."""
    return Manifold.cube((WINDOW[0] + 3, WINDOW[1] + 3, 0.6)).translate(
        (W / 2 - (WINDOW[0] + 3) / 2, SCREEN_CY - (WINDOW[1] + 3) / 2, T - WINDOW_DEPTH - 0.6))


def stick():
    """The 5-way stick's cap: a short post with a dished, knurled-looking top."""
    post = Manifold.cylinder(2.0, 2.2, circular_segments=SEG).translate((STICK_X, STICK_Y, T - 2.2))
    cap = Manifold.cylinder(3.2, STICK_KNOB / 2, STICK_KNOB / 2 - 0.6, circular_segments=SEG).translate((STICK_X, STICK_Y, T - 0.6))
    dish = Manifold.sphere(9, circular_segments=SEG).translate((STICK_X, STICK_Y, T + 2.6 + 9 - 0.5))
    ridges = [Manifold.cube((0.8, STICK_KNOB, 0.6), center=True).rotate((0, 0, a)).translate((STICK_X, STICK_Y, T + 2.2))
              for a in range(0, 180, 30)]
    knob = cap - dish
    for r in ridges:
        knob = knob - r
    return post + knob


def button():
    """The timing button: a broad cap standing 1.5 mm proud of the face."""
    return Manifold.batch_hull([
        Manifold.cylinder(0.01, BUTTON / 2, circular_segments=SEG).translate((BUTTON_X, BUTTON_Y, T - 1.6)),
        Manifold.cylinder(0.01, BUTTON / 2, circular_segments=SEG).translate((BUTTON_X, BUTTON_Y, T + 0.8)),
        Manifold.cylinder(0.01, BUTTON / 2 - 1.0, circular_segments=SEG).translate((BUTTON_X, BUTTON_Y, T + 1.5)),
    ])


# A whole broom, for scale: a 1 1/8" handle 48" (1219 mm) long, the device's top 200 mm below its
# end (under the top hand), and a head about 203 x 82 mm on a pivot at the bottom.
HANDLE_LEN, DEVICE_FROM_TOP = 1219.0, 200.0
HEAD = (203.0, 82.0, 22.0)           # length (across the handle), width, thickness of the block
PAD_T = 8.0


def handle():
    if not CONFIG.get("broom"):
        return Manifold.cylinder(240, HANDLE_R, circular_segments=SEG * 2).rotate((-90, 0, 0)).translate(
            (W / 2, -90, SADDLE_DEPTH - HANDLE_R))
    top = H + DEVICE_FROM_TOP
    return Manifold.cylinder(HANDLE_LEN, HANDLE_R, circular_segments=SEG * 2).rotate((-90, 0, 0)).translate(
        (W / 2, top - HANDLE_LEN, SADDLE_DEPTH - HANDLE_R))


def broom_head():
    """The head: a block with a pad under it, on a short pivot below the handle."""
    cz = SADDLE_DEPTH - HANDLE_R
    bottom = H + DEVICE_FROM_TOP - HANDLE_LEN
    L, Wd, Tk = HEAD
    pivot = Manifold.cylinder(22, HANDLE_R + 2, circular_segments=SEG).rotate((-90, 0, 0)).translate((W / 2, bottom - 20, cz))
    block = Manifold.cube((L, Tk, Wd), center=True).translate((W / 2, bottom - 20 - Tk / 2, cz))
    return pivot + block


def broom_pad():
    cz = SADDLE_DEPTH - HANDLE_R
    bottom = H + DEVICE_FROM_TOP - HANDLE_LEN
    L, Wd, Tk = HEAD
    return Manifold.cube((L - 4, PAD_T, Wd - 4), center=True).translate((W / 2, bottom - 20 - Tk - PAD_T / 2, cz))


def internals():
    """The PaperDIY inside, for the see-through view: the panel, the board on the back of its lower end,
    and the 140 mAh battery (its size estimated) beside it."""
    front = T - WINDOW_DEPTH                     # the glass's face, under the window's lip
    gx0 = W / 2 - GLASS[0] / 2
    panel = Manifold.cube((GLASS[0], GLASS[1], 1.2)).translate((gx0, GLASS_TOP - GLASS[1], front - 1.2))
    pb = GLASS_TOP - GLASS[1]
    board = Manifold.cube((61.15, 47.1, 4.3)).translate((W / 2 - 61.15 / 2, pb, front - 5.5))
    for a, b in TABS:   # the tabs below the panel, with USB-C in the left one
        board = board + Manifold.cube((b - a, TAB_DROP + 0.5, 3.0)).translate((gx0 + a, pb - TAB_DROP, front - 4.4))
    port_z = front - 1.2 - 2.4
    board = board + Manifold.cube((USB_W, 7.3, USB_H)).translate((gx0 + sum(USB_X) / 2 - USB_W / 2, pb - TAB_DROP, port_z - USB_H / 2))
    battery = Manifold.cube((20.0, 30.0, 4.0)).translate((W / 2 - 10.0, GLASS_TOP - GLASS[1] + 52.0, front - 5.2))
    return panel, board, battery


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
    trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts)).export(
        OUT / f"model-{LAYOUT}-{name}.stl")


if __name__ == "__main__":
    import json
    import sys
    OUT.mkdir(exist_ok=True)
    index = OUT / "layouts.json"   # kept for every layout built, so one can be rebuilt alone
    layouts = json.loads(index.read_text()) if index.exists() else {}
    for name in sys.argv[1:] or LAYOUTS:
        configure(name)
        parts = [("body", body()), ("glass", screen_bed()), ("stick", stick()), ("button", button()),
                 ("handle", handle()), ("strap", strap())]
        if CONFIG.get("broom"):
            parts += [("head", broom_head()), ("pad", broom_pad())]
            parts += list(zip(("panel", "board", "battery"), internals()))
        for part_name, part in parts:
            save(part, part_name)
        layouts[name] = dict(W=W, H=H, T=T, screen=[W / 2, SCREEN_CY], active=list(ACTIVE),
                             portrait=LAYOUTS[name]["portrait"], texture=CONFIG.get("screen"),
                             broom=bool(CONFIG.get("broom")))
        print(f"{name:9} case {W:.0f} x {H:.0f} x {T:.0f} mm; window {WINDOW[0]:.1f} x {WINDOW[1]:.1f}")
    index.write_text(json.dumps(layouts, indent=1))
