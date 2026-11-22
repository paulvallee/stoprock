"""Compare the firmware's screens with the mockups, pixel by pixel.

    cd firmware && pio run -e render && .pio/build/render/program /tmp/render
    python tools/compare_render.py /tmp/render [out.png]
"""
from pathlib import Path
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "mockups"))
import logo  # noqa: E402
import render  # noqa: E402
import screen  # noqa: E402


def main():
    src = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    shots = render.BASE + [False]
    refs = {
        "main": screen.main_screen(render.STONES, shots, tip=render.TIPS[3]),
        "battery": screen.main_screen(render.STONES, shots, tip=render.WARNING, warn=True),
        "help": screen.help_page(logo.device_logo(), "Last game 9/12 shots made", shot_pct=75, battery=84, battery_note="~11 games battery left"),
    }
    refs["competition"] = screen.competition_screen(render.COMP_STONES, render.COMP_ACTIVE)
    for a in (0, 2):
        refs[f"competition-col{a + 1}"] = screen.competition_screen(render.COMP_STONES, a)
    refs["competition-battery"] = screen.competition_screen(render.COMP_STONES, render.COMP_ACTIVE, low_battery=True)
    for n in (0, 8, 35, 63, 64):
        coach = [i % 5 != 3 and i % 7 != 5 for i in range(n)]
        st = render.COACH_STONES if n else []
        refs[f"coach1-{n:02d}"] = screen.coach_timer_screen(st, coach)
        refs[f"coach2-{n:02d}"] = screen.coach_rocks_screen(st, coach)
    for n in range(17):
        refs[f"cursor{n:02d}"] = screen.main_screen(render.STONES, [i % 3 != 2 for i in range(n)], tip=render.TIPS[0])
    rows, worst = [], 0
    for name, ref in refs.items():
        fw = Image.open(src / f"{name}.pbm").convert("1")
        diff = Image.new("RGB", (200, 200), "white")
        n = 0
        for y in range(200):
            for x in range(200):
                a, b = ref.getpixel((x, y)) == 0, fw.getpixel((x, y)) == 0
                if a != b:
                    n += 1
                    diff.putpixel((x, y), (220, 40, 40) if b else (40, 90, 220))
                elif a:
                    diff.putpixel((x, y), (190, 190, 190))
        worst = max(worst, n)
        print(f"{name:20s} {n:5d} pixels differ")
        rows.append((ref.convert("RGB"), fw.convert("RGB"), diff))
    if out:
        z = 2
        sheet = Image.new("RGB", (3 * 200 * z + 40, len(rows) * (200 * z + 20) + 20), (240, 240, 240))
        for i, row in enumerate(rows):
            for j, im in enumerate(row):
                sheet.paste(im.resize((200 * z, 200 * z), Image.NEAREST), (10 + j * (200 * z + 10), 10 + i * (200 * z + 20)))
        sheet.save(out)
        print("wrote", out)
    return 0 if worst == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
