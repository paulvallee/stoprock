# Pro mode

Features for a paid tier, unlocked by the companion app. **Coach mode is built** (switch on holding the rocker
up; see [`SPEC.md`](SPEC.md) section 3) and open to all for now; the licence check below is not built yet.

## Unlocking

The paid app unlocks pro mode over Bluetooth. Suggested approach: the app sends a licence signed with the
publisher's private key and tied to the device's unique ID; the firmware checks the signature with a public key
built into it and stores the licence in flash. No network is needed on the device, and a licence can't be copied
to another device.

## Skip / coach mode

![Skip/coach mode mockup](mockups/pro/coach-mode.png)

(Above: the first concept. As built: [`mockups/modes.png`](mockups/modes.png).)

Tracks a whole team instead of one player. Two pages; **double-press the rocker's centre** to switch (a single
press of the centre does nothing today, so it doesn't clash with hold-to-undo). Two dots top right show which
page is up.

**Page 1, timer priority.** Times across the full width (Back2Hog and Hog2Hog 100 px each), newest large with the
previous 7 below. Along the bottom, the previous and current end's 16 rocks in four columns (lead, second, third,
skip), each headed with that position's running percentage.

**Page 2, rock priority.** Only the newest times, smaller. All 64 rocks: 8 ends by four positions, two rocks
each. Along the bottom, each position's running percentage with a small pie.

**Logging.** The rocker logs made/missed in throwing order (lead, lead, second, second, third, third, skip,
skip), with the same bold-ring cursor and hold-to-undo as player mode.

**Decided and built**
- Entered by holding the rocker up while switching on.
- The databank stores all 64 rocks and the game's mode (record format version 3); the app protocol is the same.
- Times stay separate from positions, since coaches usually time both teams' stones.

**Still open**
1. Page 1 shows 6 earlier stones; two pairs to a row would show about 12, at some cost to readability.
2. At the smaller rock size the ✗ is a 4 px cross; if it doesn't read on the real screen, a filled dot for
   missed would contrast better.
3. Coach mode shows no low-battery warning during the game (only on the off page, as games or charge left).

The mockup is drawn by [`tools/mockups/coach_mode.py`](../tools/mockups/coach_mode.py).
