# CoreInk controls: a 5-way stick and a timing button, no solder

The CoreInk's back socket (2 x 8, 2.54 mm, female) carries six free inputs, enough for a 5-way stick and
a timing button; the Grove port stays free. A right-angle header plugs into the socket so the wiring lies
about 5-6 mm off the back instead of 12-14 with a straight plug. `wiring.png` shows it put together.

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
