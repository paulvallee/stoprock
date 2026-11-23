# Case

A two-part white TPU case for the CoreInk (white to match it, as some of it shows through): a **tub** (back and walls, with the button pads, strap tunnel,
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

The CoreInk's dimensions and the positions of its buttons, dial, ports and reset come from our own
record, [`../coreink/coreink.json`](../coreink/coreink.json) (readable version:
[`../coreink/DIMENSIONS.md`](../coreink/DIMENSIONS.md)); `case.py` reads it. They were measured from
M5Stack's 3D model of the CoreInk's enclosure (not kept here) and agree with M5Stack's dimension
drawing. Given that model as `render/model-coreink.stl`, the `fit-*` views in `render/` show it in
the case; it sits with no overlap. The first prints are for fit: expect to tune the 0.2 mm fit gap,
the button nubs and the snap bead.

The concept sketch puts the power button on the left; the drawing shows it on the right, below the
dial, and the model follows the drawing.

**Mount.** One tunnel runs across the back (21 x 2 mm, open at both sides; the TPU stretches for a
3 mm cord). A lanyard cord through it, its ends meeting at a sliding cord-lock bead above the case,
hangs the case upright and centred. On a broom, a silicone band (about 20 x 1 mm) goes through the
tunnel and round the handle, and its ends are overlapped and bonded with a silicone adhesive, the
overlap lying doubled inside the tunnel; the case and band slide on and off over the top of the
handle. The back is flat (it's what the tub prints standing on), with the stoprock wordmark
debossed 0.5 mm below the tunnel. The reset pinhole goes through the back and the tunnel: slide the
band or cord aside to use it. The mount replaced a corner lanyard loop.
