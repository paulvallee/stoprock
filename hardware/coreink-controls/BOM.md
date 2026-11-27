# Bill of materials: CoreInk controls case

The concept in `case.png` and `stick-cap.png`: a CoreInk in a broom case with a 5-way stick and a timing
button stacked in a tail. Prices in Canadian dollars, before tax and shipping, as of 2026-11-27. The
CoreInk is Mouser's Canadian price. The other parts were listed in US dollars and are converted at
1.42 CAD per USD (mid-November 2026), so they will move with the exchange rate. "Pack" is the smallest
you can buy; "per unit" is its share when building several.

## Parts

| # | Part | Source | Pack (CAD) | Per unit (CAD) | Notes |
|---|---|---|---|---|---|
| 1 | M5Stack CoreInk (ESP32, 1.54" e-ink) | [Mouser](https://www.mouser.ca/c/?q=CoreInk) | 43.00 | 43.00 | If you already have one, leave it out |
| 2 | Right-angle header, 2 x 40, 2.54 mm: Sullins **PRPC040DBAN-M71RC** | [DigiKey](https://www.digikey.ca/en/products/detail/sullins-connector-solutions/PRPC040DBAN-M71RC/2775654) | about 2.95 (US$2.08) | 0.59 | One strip makes five 2 x 8 pieces; snip the 8 pins not used |
| 3 | 5-way switch breakout, 10 kOhm pull-ups on board (twin pack) | [PMD Way](https://pmdway.com/products/5-way-tactile-switch-breakout) | 23.33 (US$16.43) | 11.67 | Free delivery. Header pins come loose: solder the wires to its pads instead |
| 4 | Adafruit **1119**, 12 mm tactile switch (10 pack) | [DigiKey](https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/2325/1119_Web.pdf) / [Adafruit](https://www.adafruit.com/product/1119) | 3.55 (US$2.50) | 0.36 | The timing button |
| 5 | Adafruit **1951**, female/female jumper wires, 20 x 75 mm | [Adafruit](https://www.adafruit.com/product/1951) / [DigiKey](https://www.digikey.com/en/products/detail/adafruit-industries-llc/1951/6827085) | 2.77 (US$1.95) | 0.55 | 4 per unit: cut each in half for 8 leads, plug end at the CoreInk, cut end soldered at the controls |
| 6 | Case body (one piece in the concept; to be split into a back and a front) | print shop, nylon (SLS PA12) | | 14-28 (estimate) | 46.2 cm3 in the model |
|   | ... or printed yourself, PETG | | | 1.50-3 (material) | |
| 7 | Stick cap and button cap | TPU (stick), any (button) | | under 1.50 | 0.6 cm3 each |
| 8 | Strap, hook-and-loop, about 16 mm wide, 2 x 25 cm | hardware store | | about 1.50 (estimate) | Through the two tunnels, round the handle |
| 9 | Small screws to close the case (M2, about 4) | | | about 0.45 (estimate) | The opening isn't designed yet |
| 10 | Solder, heat-shrink, a dab of hot glue | consumables | | | |

## Totals (CAD)

| | With a print-shop case | Printing the case yourself |
|---|---|---|
| **The first unit**, buying the smallest packs (spares left over: 4 headers, a second 5-way board, 9 switches, 16 leads) | about **93-107** | about **80-82** |
| **Each unit** when building several (pack prices shared out) | about **73-87** | about **61-62** |
| **The controls and case only**, per unit, for a CoreInk you already have | about **30-44** | about **18-19** |

Plus shipping: DigiKey Canada ships free over 100 CAD; PMD Way is free; check Mouser's threshold. Adafruit
parts are also sold through distributors, so the header, switches and leads may fit in the Mouser order.

## Labour

Nine solder joints (7 pads on the 5-way board, 2 legs on the switch), the header cut and plugged in, the
leads routed down the conduit: about 30-45 minutes the first time.
