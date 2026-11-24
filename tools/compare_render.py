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
    ls, lme, lact = render.LEAGUE_STONES, render.LEAGUE_ME, render.LEAGUE_ACTIVE
    refs = {
        "league": screen.league_screen(ls, shots, lme, lact),
        "league-battery": screen.league_screen(ls, shots, lme, lact, low_battery=True),
        "help": screen.help_page(logo.device_logo(), "Last game 9/12 shots made", shot_pct=75, battery=84, battery_note="~11 games battery left"),
    }
    refs["competition-tip"] = screen.competition_screen([], 0, 0, tip=True)
    refs["league-tip"] = screen.league_screen([], [], 2, 0, tip=True)
    refs["coach1-tip"] = screen.coach_timer_screen([], [], tip=True)
    refs["competition-undo"] = screen.competition_screen(ls, lme, lact, tip="undo")
    refs["league-undo"] = screen.league_screen(ls, shots, lme, lact, tip="undo")
    refs["coach1-undo"] = screen.coach_timer_screen(render.COACH_STONES, render.COACH_SHOTS, tip="undo")
    refs["coach1-rocks"] = screen.coach_timer_screen(render.COACH_STONES, render.COACH_SHOTS, focus="rocks")
    refs["coach2-rocks"] = screen.coach_rocks_screen(render.COACH_STONES, render.COACH_SHOTS, focus="rocks")
    refs["competition-move"] = screen.competition_screen(ls, lme, lact, tip="move")
    refs["league-move"] = screen.league_screen(ls, shots, lme, lact, tip="move")
    refs["coach1-move"] = screen.coach_timer_screen(render.COACH_STONES, render.COACH_SHOTS, tip="move")
    refs["coach2-move"] = screen.coach_rocks_screen(render.COACH_STONES, render.COACH_SHOTS, tip="move")
    refs["league-off"] = screen.league_screen(ls, shots, lme, lact, tip="off")
    refs["coach2-off"] = screen.coach_rocks_screen(render.COACH_STONES, render.COACH_SHOTS, tip="off")
    refs["coach2-undo"] = screen.coach_rocks_screen(render.COACH_STONES, render.COACH_SHOTS, tip="undo")
    refs["competition-wait-b2h"] = screen.competition_screen(ls, lme, lact, waiting="b2h")
    refs["league-rocks"] = screen.league_screen(ls, shots, lme, lact, waiting="b2h", focus="rocks")
    lhalf = [(ls[0][0], None, ls[0][2])] + list(ls[1:])
    refs["league-wait-h2h"] = screen.league_screen(lhalf, shots, lme, 3, waiting="h2h")
    refs["league-wait-h2h-only"] = screen.league_screen(ls, shots, lme, 3, waiting="h2h-only")
    refs["competition-wait-h2h-only"] = screen.competition_screen(ls, lme, 3, waiting="h2h-only")
    refs["coach2-wait-h2h-only"] = screen.coach_rocks_screen(render.COACH_STONES, render.COACH_SHOTS,
                                                             waiting="h2h-only")
    cs = render.COACH_STONES
    refs["coach1-wait-b2h"] = screen.coach_timer_screen(cs, render.COACH_SHOTS, waiting="b2h")
    refs["coach2-wait-h2h"] = screen.coach_rocks_screen([(cs[0][0], None)] + cs[1:], render.COACH_SHOTS, waiting="h2h")
    for n in range(17):  # the cursor at every position; each position as yours, each column active
        refs[f"league{n:02d}"] = screen.league_screen(ls, [i % 3 != 2 for i in range(n)], n % 4, n % 4,
                                                      focus="rocks" if n % 2 else "players")
    refs["competition"] = screen.competition_screen(ls, lme, lact)
    refs["competition-battery"] = screen.competition_screen(ls, lme, lact, low_battery=True)
    for me in range(4):  # each position as yours, the cursor on each column
        refs[f"competition-me{me}"] = screen.competition_screen(ls, me, (me + 1) % 4)
    for n in (0, 8, 35, 63, 64):
        coach = [i % 5 != 3 and i % 7 != 5 for i in range(n)]
        st = render.COACH_STONES if n else []
        refs[f"coach1-{n:02d}"] = screen.coach_timer_screen(st, coach, active=n % 5)
        refs[f"coach2-{n:02d}"] = screen.coach_rocks_screen(st, coach)
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
