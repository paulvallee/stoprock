# Bill of materials: CoreInk controls case

The concept in `case.png` and `stick-cap.png`: a CoreInk in a broom case with a 5-way stick and a timing
button stacked in a tail. Prices in US dollars, as listed on 2026-11-27, before tax and shipping. "Pack"
is the smallest you can buy; "per unit" is its share when building several.

## Parts

| # | Part | Source | Pack price | Per unit | Notes |
|---|---|---|---|---|---|
| 1 | M5Stack CoreInk (ESP32, 1.54" e-ink) | [M5Stack](https://shop.m5stack.com/products/m5stack-esp32-core-ink-development-kit1-54-elnk-display) | 27.21 | 27.21 | If you already have one, leave it out |
| 2 | Right-angle header, 2 x 40, 2.54 mm: Sullins **PRPC040DBAN-M71RC** | [DigiKey](https://www.digikey.ca/en/products/detail/sullins-connector-solutions/PRPC040DBAN-M71RC/2775654) | about 2.08 | 0.42 | One strip makes five 2 x 8 pieces; snip the 8 pins not used |
| 3 | 5-way switch breakout, 10 kOhm pull-ups on board (twin pack) | [PMD Way](https://pmdway.com/products/5-way-tactile-switch-breakout) | 16.43 | 8.22 | Free delivery. Header pins come loose: solder the wires to its pads instead |
| 4 | Adafruit **1119**, 12 mm tactile switch (10 pack) | [DigiKey](https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/2325/1119_Web.pdf) / [Adafruit](https://www.adafruit.com/product/1119) | 2.50 | 0.25 | The timing button |
| 5 | Adafruit **1951**, female/female jumper wires, 20 x 75 mm | [Adafruit](https://www.adafruit.com/product/1951) / [DigiKey](https://www.digikey.com/en/products/detail/adafruit-industries-llc/1951/6827085) | 1.95 | 0.39 | 4 per unit: cut each in half for 8 leads, plug end at the CoreInk, cut end soldered at the controls |
| 6 | Case body (one piece in the concept; to be split into a back and a front) | print shop, nylon (SLS PA12) | | 10-20 (estimate) | 46.2 cm3 in the model |
|   | ... or printed yourself, PETG | | | 1-2 (material) | |
| 7 | Stick cap and button cap | TPU (stick), any (button) | | under 1 | 0.6 cm3 each |
| 8 | Strap, hook-and-loop, about 16 mm wide, 2 x 25 cm | hardware store | | about 1 (estimate) | Through the two tunnels, round the handle |
| 9 | Small screws to close the case (M2, about 4) | | | about 0.30 (estimate) | The opening isn't designed yet |
| 10 | Solder, heat-shrink, a dab of hot glue | consumables | | | |

## Totals

| | With a print-shop case | Printing the case yourself |
|---|---|---|
| **The first unit**, buying the smallest packs (spares left over: 4 headers, a second 5-way board, 9 switches, 16 leads) | about **62-72** | about **53-54** |
| **Each unit** when building several (pack prices shared out) | about **48-58** | about **39-40** |
| **The controls and case only**, for a CoreInk you already have | about **21-31** | about **12-13** |

Plus shipping: DigiKey Canada ships free over 100 CAD; PMD Way is free; M5Stack charges shipping.

## Labour

Nine solder joints (7 pads on the 5-way board, 2 legs on the switch), the header cut and plugged in, the
leads routed down the conduit: about 30-45 minutes the first time.
