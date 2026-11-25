# 2.66" broom-mounted concept

A look at a stoprock built round a 2.66" e-paper screen (Good Display, 296 x 152, 71.8 x 36.3 mm glass,
60.1 x 30.7 mm active area) instead of the CoreInk, mounted on the broom like a Rockwatcher: the
screen upright on the face, an Alps 5-way stick (up, down, left, right, press) and one big timing
button at the bottom, **stacked** (48 x 125 mm, the chosen layout: centred on the broom, the stick above, the timing
button on the end) or side by side (50 x 109 mm, for comparison), a saddle on the back that sits on the handle, a strap through two
tunnels, USB-C at the top end. For looking at, not printing: the inside (board, battery, switches'
mounting) isn't designed yet.

| File | |
|---|---|
| `concept.py` | The model, in code (manifold3d): `python concept.py [stack|row|landscape]` |
| `screen_mock_portrait.py`, `screen_mock.py` | Rough 152 x 296 and 296 x 152 screens in our fonts, for the renders |
| `concept-portrait.png` | The two upright layouts, captioned |
| `concept-paperdiy.png` | The same stacked layout round M5Stack's PaperDIY kit (4.7", 540 x 960), at its real size (69 x 163 x 15 mm), on a whole broom |
| `screen_mock_paperdiy.py` | A rough 540 x 960 screen for it |
| `concept-landscape.png` | The first look, with the screen across the top (84 x 80 mm) |
| `render/` | How the renders are made |

## PaperDIY

`python concept.py paperdiy` models the case round M5Stack's PaperDIY Assembly Kit (K159) from its
dimension drawing: the 62.5 x 115.3 x 1.2 mm panel, the 61 x 47 mm board on the back of its lower end
(5.5 mm in all), the included 140 mAh battery (its size estimated), and no front window. The screen
window is the visible area of M5Stack's own enclosure for the kit (59.6 x 105.1 mm, in their
M5_Hardware repository). The broom is a 1 1/8" handle, 48" long, with a head about 203 x 82 mm
(retail listings), the case's top 200 mm below the handle's end.
