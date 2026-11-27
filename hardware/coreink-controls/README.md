# CoreInk controls: a 5-way stick and a timing button, no solder

The CoreInk's back socket (2 x 8, 2.54 mm, female) carries six free inputs, enough for a 5-way stick and
a timing button; the Grove port stays free. A right-angle header plugs into the socket so the wiring lies
about 5-6 mm off the back instead of 12-14 with a straight plug. `wiring.png` shows it put together;
`case.png` shows a broom case for it (`case.py`); [`BOM.md`](BOM.md) lists the parts and costs.

## Parts

| Part | Notes |
|---|---|
| Right-angle header, 2 rows, 2.54 mm: Sullins **PRPC040DBAN-M71RC** (2 x 40, [DigiKey](https://www.digikey.ca/en/products/detail/sullins-connector-solutions/PRPC040DBAN-M71RC/2775654)) | Cut a 2 x 8 piece; snip off the 8 pins not used. **D**BAN is 2 rows; **S**BAN (1 row) is not it. |
| Female jumper leads, short (about 5 cm) | Their plugs slide onto the header's bent pins |
| 5-way stick module **with a VCC pin** | Its pull-ups on board, for pins 34 and 36 (no built-in pull-ups); otherwise two 10 kOhm resistors |
| Timing button | A 12 mm tactile switch with a 16 mm cap, on a small board with header pins |

## Pins

From M5Stack's CoreInk schematic (BUS1, M5_BUS_16P). Check the socket's pin order against the CoreInk's
own marking before plugging in.

| Socket pin | Signal | Goes to |
|---|---|---|
| 16 | GPIO25 | timing button (built-in pull-up; wakes the chip) |
| 7 | GPIO13 | stick up |
| 8 | GPIO14 | stick down |
| 14 | GPIO26 | stick left |
| 12 | GPIO36 | stick right (no built-in pull-up) |
| 13 | GPIO34 | stick centre (no built-in pull-up) |
| 1 | GND | stick COM, the button's other side |
| 6 | 3.3 V | the stick module's VCC (its pull-ups) |

Not used, snip them off: 2 (battery), 3 (5 V in), 4 (5 V out), 5 (**reset**), 9 and 10 (GPIO21/22, the
clock chip's I2C), 11 and 15 (GPIO18/23, the screen's SPI).

Jumper plugs on bent pins can slide off under sweeping: once it works, a dab of hot glue over the row.

## Renders

`render/`: three.js and Playwright, as `hardware/case/render`. It needs M5Stack's CoreInk model
(`model-coreink.stl`, not committed) and the screen picture, copied from `hardware/case/render/`:

```sh
cd render && npm install three@0.170.0
cp ../../case/render/model-coreink.stl ../../case/render/screen.png .
python3 -m http.server 8379 &
node shoot.js && python3 sheet.py     # -> ../wiring.png
```

## The case (concept)

`case.py` models a broom case round it: **45 x 114 x 29.5 mm**, the stick and the timing button stacked
in a 30 mm tail (18.5 mm thick) below the CoreInk, the saddle on the back sitting on a 1 1/8" handle, a
strap through two tunnels under the wiring. The CoreInk sits **upside down**: its USB-C port (on its
bottom edge) is then at the top of the case, open for charging, where the tail would otherwise cover it.
The firmware turns the screen 180 degrees and swaps the dial's up and down. The dial and the side button
end up on the case's left side, both reachable; the top button ends up under the tail, covered (the stick
and the new timing button take over).

Inside (the `cut`, `cut-tail` and `xray` views):

- the CoreInk sits on a solid back, hollowed only where the right-angle header and its plugs are;
- a **conduit** runs from there down through the shoulder into the tail;
- **PMD Way's 5-way breakout** (20.3 x 19 mm, 10 kOhm pull-ups on board) sits on four posts with pegs
  through its corner holes, turned round so its pads face the conduit (the firmware maps the directions
  back); the wires are soldered to its pads from behind, no header on it;
- the **Adafruit 1119** tactile switch sits on a shelf, two wires soldered to its legs;
- printed caps: the stick's on the switch's stem, the 16 mm button's on the switch's actuator.

The stick's cap is PS2-style but sized to the switch's short travel (`stick-cap.png`): 13 mm across, a
dished top, a ribbed rim, printed in TPU for a soft grip, standing about 5 mm proud. The switch registers a
tilt of about 3 degrees, about 0.4 mm at the cap's rim; a low collar round it, 1 mm clear, guards its edge
from a glove and stops it tilting much further.

Soldering: 7 pads on the 5-way board and 2 legs on the switch, 9 joints in all. The CoreInk end plugs
in (jumper plugs on the right-angle header).

Thickness, back to front: 6.5 (the saddle, and the strap tunnels under the wiring) + 5.5 (the header and
its plugs) + 16 (the CoreInk) + 1.5 (the frame). Still to work out: how it opens (likely a back shell
and a front frame screwed together at the CoreInk's back), and how the CoreInk is held.

Renders: `python3 case.py`, then in `render/` `node shoot-case.js && python3 sheet_case.py`.
