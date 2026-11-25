"""Compare the firmware's screens with the mockups, pixel by pixel.

    cd firmware && pio run -e render && .pio/build/render/program /tmp/render
    python tools/compare_render.py /tmp/render [out.png]
"""
from pathlib import Path
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "mockups"))
import render  # noqa: E402
import screen  # noqa: E402
import tutorials  # noqa: E402


def main():
    src = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    shots = render.BASE + [False]
    ls, lme, lact = render.LEAGUE_STONES, render.LEAGUE_ME, render.LEAGUE_ACTIVE
    refs = {
        "league": screen.league_screen(ls, shots, lme, lact),
        "league-battery": screen.league_screen(ls, shots, lme, lact, low_battery=True),
        "off-page": screen.off_page(),
        "off-page-games": screen.off_page(84, 11),
        "off-page-one-game": screen.off_page(16, 1),
        "off-page-charge-soon": screen.off_page(8, 0),
        "off-page-charge-percent": screen.off_page(84),
    }
    refs["competition-tip"] = screen.competition_screen([], 0, 0, tip=True)
    refs["league-tip"] = screen.league_screen([], [], 2, 0, tip=True)
    for i in range(len(screen.MENU_ITEMS)):
        refs[f"menu{i}"] = screen.mode_menu(i)
    for i in range(len(screen.TUTORIAL_MENU_ITEMS)):
        refs[f"tutorial-menu{i}"] = screen.mode_menu(i, tutorials=True)
    mixed = (3, 4, 2, 0, 1)
    for name, levels, muted in (("sounds", (3,) * 5, False), ("sounds-mixed", mixed, False),
                                ("sounds-muted", mixed, True)):
        for i in range(len(screen.SOUND_NAMES) + 2):
            refs[f"{name}{i}"] = screen.sounds_menu(i, levels, muted)
    for mode, name in enumerate(tutorials.MODES):
        for page in range(tutorials.page_count(mode)):
            refs[f"tutorial-{name.lower()}-{page + 1:02d}"] = tutorials.page_image(mode, page)
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
    refs["coach1-page"] = screen.coach_timer_screen(render.COACH_STONES, render.COACH_SHOTS, tip="page")
    refs["coach2-page"] = screen.coach_rocks_screen(render.COACH_STONES, render.COACH_SHOTS, tip="page")
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
    refs["league-history"] = screen.league_screen(ls, shots, lme, 3, scroll=2, scrolling=True)
    refs["coach1-history"] = screen.coach_timer_screen(cs, [True, False, True, True, False, True], active=4,
                                                       scroll=2, scrolling=True)
    refs["competition-them-right"] = screen.competition_screen(ls, lme, 3)
    refs["competition-columns-right"] = screen.competition_screen(ls, lme, lact, first_column=1)
    refs["competition-history"] = screen.competition_screen(ls, lme, 3, scroll=2, scrolling=True)
    refs["competition-history-top"] = screen.competition_screen(ls, lme, 3, scrolling=True)
    refs["competition-battery"] = screen.competition_screen(ls, lme, lact, low_battery=True)
    for me in range(4):  # each position as yours, the cursor on each column
        refs[f"competition-me{me}"] = screen.competition_screen(ls, me, (me + 1) % 4)
    for n in (0, 8, 35, 63, 64):
        coach = [i % 5 != 3 and i % 7 != 5 for i in range(n)]
        st = render.COACH_STONES if n else []
        refs[f"coach1-{n:02d}"] = screen.coach_timer_screen(st, coach, active=n % 5)
        refs[f"coach2-{n:02d}"] = screen.coach_rocks_screen(st, coach)
    slow = [(3.52, 20.00, 4)] + list(ls[1:])  # an H2H of 20.00s: the narrower fonts
    refs["league-20"] = screen.league_screen(slow, shots, lme, 3)
    refs["competition-20"] = screen.competition_screen(slow, lme, 3)
    refs["coach1-20"] = screen.coach_timer_screen([(3.52, 20.00, 4)] + list(render.COACH_STONES[1:]), [], active=4)
    refs["basic-intro"] = screen.basic_intro()
    refs["basic-empty"] = screen.basic_screen([])
    refs["basic-started"] = screen.basic_screen([], waiting="b2h")
    refs["basic"] = screen.basic_screen(ls)
    refs["basic-wait-b2h"] = screen.basic_screen(ls, waiting="b2h")
    refs["basic-wait-h2h-only"] = screen.basic_screen(ls, waiting="h2h-only")
    refs["basic-wait-h2h"] = screen.basic_screen(lhalf, waiting="h2h")
    refs["basic-20"] = screen.basic_screen(slow, low_battery=True, tip="undo")
    refs["basic-few"] = screen.basic_screen(ls[:2], tip="off")
    refs["basic-scroll4"] = screen.basic_screen(ls, scroll=4)
    refs["basic-scroll-end"] = screen.basic_screen(ls, low_battery=True, scroll=99)
    refs["basic-scroll-timing"] = screen.basic_screen(ls, waiting="b2h", scroll=2)
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
