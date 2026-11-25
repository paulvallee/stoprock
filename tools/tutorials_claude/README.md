# Tutorial cards: Claude's proposal

The tutorial cards shown by the device's Tutorials menu. `python3 tools/tutorials.py` stores them in
the firmware (`firmware/lib/stoprock_ui/src/tutorial_data.cpp`), each card whole, about 5 KB.

    python3 tools/tutorials_claude/build.py

- `cards/`: every card, 200 x 200, 1-bit, as the CoreInk would show it (`<deck>-<nn>-<card>.png`).
- `sheets/`: each deck enlarged 3x on the e-ink tint, for reviewing.

## Decks

| Deck | Cards |
|---|---|
| Basic (8) | Timing buttons, Time a stone, Hog to hog only, Read the times, Older stones, Undo, Turn off, Skip the menu |
| Competition (11) | Timing buttons, Time a stone, Hog to hog only, Read the times, Who's throwing, Wrong player?, Older times, Your position, Undo, Turn off, Skip the menu |
| League (12) | as Competition, plus Log your shot |
| Coach (12) | as League, with Two pages instead of Your position |

Timing first (Hog to hog only right after Time a stone, as a way to time one), then reading and the rocker, then power.

## Copy

| Card | Lines |
|---|---|
| Timing buttons | Press the top or side / button at each line. |
| Time a stone | Press at back or tee line, / then at each hog line. |
| Read the times | The big numbers show the / last stone you timed. |
| Who's throwing | The highlight moves by / itself, or use the rocker. |
| Wrong player? | Hold up to move the / last time one player left. |
| Log your shot | Hold down twice for rocks: / Up: made. Down: missed. |
| Older times | Hold down to see older / times. Hold up to return. |
| Older stones (Basic) | Down: older stones. / Up: newer stones. |
| Your position | Double-click the middle / to change your position. |
| Two pages (Coach) | Double-click the middle / to switch times and rocks. |
| Undo | Hold the middle 1 second / to undo a time or shot. (Basic, Competition: the last time.) |
| Turn off | Hold the side button / 3 seconds to turn off. |
| Skip the menu | Hold the rocker as you / turn it on to skip the menu. |
| Hog to hog only | Start at the near hog? / After 7 s it times H2H. |

## Design rules

- One idea a card; at most two lines of copy (Nunito Bold 15), each checked to fit.
- Words used the same way everywhere: press, hold, double-click, the middle (of the rocker), time.
- Drawn as the screen draws it: B2H black on white, H2H white on black; made a filled rock, missed a
  dot in a ring; the selected player's header inverted; the big times in the device's DSEG digits.
- The device drawn from `hardware/coreink/coreink.json`, every button where it really is; the rocker
  labelled but left outlined, so it isn't mistaken for a timing button.
- Shapes and arrows drawn at 4x and reduced to 1 bit (smooth curves); text drawn at 1x in 1 bit (crisp).
- Header: the deck, "3/12", the title. Footer: ▲ Back (not on the first card),
  ● Done, ▼ Next (not on the last).

## Ideas taken from the alternative set (assets/screens/codex)

Reviewed against it and kept only what was better: the start-up shortcuts (Skip the menu), the
hog-to-hog-only tip, "the highlight moves by itself", "hold down twice" for the rocks, "keep holding
to undo more", the lines named in full (Back line, Near hog, Far hog), and "3/12" in place of progress
dots. Not taken: three- and four-line copy in mixed small sizes, the shrunken-screenshot "Read the
screen" card (illegible at size), and straight hairline arrows.
