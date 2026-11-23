# Case

A two-part TPU case for the CoreInk: a **tub** (back and walls, with the button pads, strap tunnel,
rocker cut-out, USB-C opening and reset pinhole) and a **bezel** (the front frame, with a faceted cut-gem front) that snaps into a
groove in the tub's rim. Concept sketch: [`docs/mockups/case-concept.png`](../../docs/mockups/case-concept.png).

| File | |
|---|---|
| `case.py` | The model, in code: change a number, run it, get new STLs |
| `stoprock-case-tub.stl`, `stoprock-case-bezel.stl` | Printable parts, oriented for printing |
| `preview.png` | Renders of both parts (made with `render/`) |
| `mount-concept.png` | The same case on a cord lanyard and on a broom (silicone band) |
| `band-section.png` | Section through the tunnel: the band's bonded overlap inside it |
| `QUOTE.md` | What to send a print shop with the STLs |
| `stoprock-case-quote.zip` | STLs + preview + QUOTE.md, ready to attach |

Positions come from M5Stack's CoreInk dimension drawing. Before printing for use, check on the
device the values marked `MEASURE` in `case.py`: where the buttons, dial and USB-C sit through the
thickness, and which side button is power. The first prints are for fit: expect to
tune the 0.2 mm fit gap, the button nubs and the snap bead.

The concept sketch puts the power button on the left; the drawing shows the side button on the
right, below the dial, and the model follows the drawing.

**Mount.** One tunnel runs across the back (21 x 2 mm, open at both sides; the TPU stretches for a
3 mm cord). A lanyard cord through it, its ends meeting at a sliding cord-lock bead above the case,
hangs the case upright and centred. On a broom, a silicone band (about 20 x 1 mm) goes through the
tunnel and round the handle, and its ends are overlapped and bonded with a silicone adhesive, the
overlap lying doubled inside the tunnel; the case and band slide on and off over the top of the
handle. Two low rails keep the case square on the handle. It replaced a corner lanyard loop.
