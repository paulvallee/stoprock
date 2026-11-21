"""Check the shots cursor at every position: it must stay inside its own slot.

    python3 tools/check_cursor.py [sheet.png]

For 0-16 rocks logged, draws the main screen with and without the cursor; the pixels the
cursor adds must all lie within the slot (radius 7.5 of the rock's centre), so it can never
touch an end number, a neighbouring rock, a divider or the screen edge. With 16 logged
there must be no cursor at all. Optionally saves a sheet of all 17 screens' shots columns.
"""
from pathlib import Path
import sys

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent / "mockups"))
import render  # noqa: E402
import screen  # noqa: E402


def main():
    failures = 0
    crops = []
    for n in range(17):
        shots = [i % 3 != 2 for i in range(n)]   # a mix of made and missed
        with_c = screen.main_screen(render.STONES, shots, tip=render.TIPS[0])
        without = screen.main_screen(render.STONES, shots, tip=render.TIPS[0], show_cursor=False)
        added = [(x, y) for y in range(200) for x in range(200)
                 if with_c.getpixel((x, y)) != without.getpixel((x, y))]
        if n == 16:
            ok = not added
            detail = "no cursor when all 16 are logged" if ok else f"{len(added)} cursor pixels on a full game"
        else:
            cx, cy = screen.rock_xy(n)
            cy = int(cy)
            outside = [(x, y) for x, y in added if (x - cx) ** 2 + (y - cy) ** 2 > 7.5 ** 2]
            ok = bool(added) and not outside
            detail = f"slot {n + 1:2d} (end {n // 2 + 1}, rock {n % 2 + 1}): {len(added)} px" + \
                     ("" if ok else f", {len(outside)} outside the slot" if added else ", cursor missing")
        print(("ok   " if ok else "FAIL ") + detail)
        failures += not ok
        crops.append(with_c.crop((screen.SH_X0 - 2, 0, 200, 200)))
    if len(sys.argv) > 1:
        z = 3
        w = crops[0].width * z
        sheet = Image.new("RGB", (len(crops) * (w + 6), 200 * z), (240, 240, 240))
        for i, c in enumerate(crops):
            sheet.paste(screen.to_rgb(c, z), (i * (w + 6), 0))
        sheet.save(sys.argv[1])
        print("wrote", sys.argv[1])
    print("all positions ok" if not failures else f"{failures} position(s) failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
