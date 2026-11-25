# Tutorial cards: Claude's proposal

A revised set of tutorial cards, for comparison with the current ones (`tools/tutorials.py`) and an
alternative set. Nothing here is wired into the firmware yet.

    python3 tools/tutorials_claude/build.py

- `cards/`: every card, 200 x 200, 1-bit, as the CoreInk would show it (`<deck>-<nn>-<card>.png`).
- `sheets/`: each deck enlarged 3x on the e-ink tint, for reviewing.

## Decks

| Deck | Cards |
|---|---|
| Basic (6) | Timing buttons, Time a stone, Read the times, Older stones, Undo, Turn off |
| Competition (9) | Timing buttons, Time a stone, Read the times, Who's throwing, Wrong player?, Older times, Your position, Undo, Turn off |
| League (10) | as Competition, plus Log your shot |
| Coach (10) | as League, with Two pages instead of Your position |

## Copy

| Card | Lines |
|---|---|
| Timing buttons | Press the top or side / button at each line. |
| Time a stone | Press at the back line, / then at each hog line. |
| Read the times | The newest stone, big: / back to hog, hog to hog. |
| Who's throwing | New times go to the / highlighted player. |
| Wrong player? | Hold up to move the / last time one player left. |
| Log your shot | On your rocks: up = made, / down = missed. |
| Older times | Hold down to see older / times. Hold up to return. |
| Older stones (Basic) | Down: older stones. / Up: newer stones. |
| Your position | Double-click the middle / to change your position. |
| Two pages (Coach) | Double-click the middle / to switch times and rocks. |
| Undo | Hold the middle 1 second / to undo a time or shot. (Basic, Competition: the last time.) |
| Turn off | Hold the side button / 3 seconds to turn off. |

## Design rules

- One idea a card; at most two lines of copy (Nunito Bold 15), each checked to fit.
- Words used the same way everywhere: press, hold, double-click, the middle (of the rocker), time.
- Drawn as the screen draws it: B2H black on white, H2H white on black; made a filled rock, missed a
  dot in a ring; the selected player's header inverted; the big times in the device's DSEG digits.
- The device drawn from `hardware/coreink/coreink.json`, every button where it really is; the rocker
  labelled but left outlined, so it isn't mistaken for a timing button.
- Shapes and arrows drawn at 4x and reduced to 1 bit (smooth curves); text drawn at 1x in 1 bit (crisp).
- Header: the deck, a dot a card (this one filled), the title. Footer: ▲ Back (not on the first card),
  ● Done, ▼ Next (not on the last).

## Firmware changes these assume

- Down on the last card should do nothing visible (or return to the list), since there's no Next.
- "Who's throwing" and "Wrong player?" describe up as left (as the firmware now does).
