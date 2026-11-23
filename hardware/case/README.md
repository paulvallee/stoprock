# Case

A two-part TPU case for the CoreInk: a **tub** (back and walls, with the button pads, lanyard loop,
rocker cut-out, USB-C opening and reset pinhole) and a **bezel** (the front frame) that snaps into a
groove in the tub's rim. Concept sketch: [`docs/mockups/case-concept.png`](../../docs/mockups/case-concept.png).

| File | |
|---|---|
| `case.py` | The model, in code: change a number, run it, get new STLs |
| `stoprock-case-tub.stl`, `stoprock-case-bezel.stl` | Printable parts, oriented for printing |
| `preview.png` | Renders of both parts (made with `render/`) |
| `QUOTE.md` | What to send a print shop with the STLs |
| `stoprock-case-quote.zip` | STLs + preview + QUOTE.md, ready to attach |

Positions come from M5Stack's CoreInk dimension drawing. Before printing for use, check on the
device the values marked `MEASURE` in `case.py`: where the buttons, dial and USB-C sit through the
thickness, the green LED, and which side button is power. The first prints are for fit: expect to
tune the 0.2 mm fit gap, the button nubs and the snap bead.

The concept sketch puts the power button on the left; the drawing shows the side button on the
right, below the dial, and the model follows the drawing.
