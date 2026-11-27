# Hardware concepts, parked for review

Ideas explored but not decided. Each folder has its own README, model (code) and renders.

| Concept | Folder | Status |
|---|---|---|
| **CoreInk + 5-way stick + timing button**, no solder: a right-angle header in the CoreInk's back socket, short jumper leads; a broom case with the controls stacked in a tail, the CoreInk upside down so USB-C is open at the top (45 x 114 x 29.5 mm, PMD Way 5-way board and Adafruit 1119 inside) | [`coreink-controls/`](coreink-controls/) (`wiring.png`, `case.png`) | **Parked for review.** Parts identified (Sullins PRPC040DBAN-M71RC header, a 5-way module with a VCC pin, a 12 mm button). Firmware not started, on purpose: turn the screen 180 degrees, swap the dial's up and down, add the stick (GPIO 13, 14, 26, 36, 34) and the timing button (GPIO 25). |
| **M5Stack PaperDIY** (4.7", 540 x 960) on the broom: the stick and timing button stacked in a 30 mm tail, USB-C and the power button under the shoulders (69 x 160 x 15 mm) | [`concept-266/`](concept-266/) (`concept-paperdiy.png`) | Parked. The kit has no buzzer or clock chip; its pins, power chip and dimensions are in that README. |
| **2.66" e-paper** (296 x 152) on the broom, upright, controls stacked | [`concept-266/`](concept-266/) (`concept-portrait.png`) | Parked; superseded by the two above. |

The current case for the CoreInk as it is: [`case/`](case/).
