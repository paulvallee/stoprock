"""Check the throw order's assumptions against real games.

    python tools/analyze_games.py games.txt [more files...]

Reads games exported from the device (the "@GAMES" listing of the `games` command, or plain JSON
lines; other lines are ignored) and, for competition and league games recorded with format
version 5 or later, prints:

- each timed stone: when it started, the gap since the previous stone (or your logged shot, in
  league), who the device guessed, and who it ended up under (a guess that differs is one you
  corrected: move wrong times before exporting, as the final column is taken as the truth);
- how often the guess was right, by gap;
- for ends timed from their first stone with nothing missing but your own throws (competition and
  league can't time you): the time from each stone's start to the next, by who threw the next one,
  against the expected times in throw_order.h (kIntervalS by position, kSkipSwapS before the first
  skip stone, kEndBreakS between ends). These are the numbers to tune;
- the gaps between stones that look a throw apart (the other team's stone straight after ours, or
  ours after theirs, under 150 s);
- league: how long after your logged shot the next stone started (the device takes your throw as
  20 s before you log it).
"""
import json
import re
import statistics
import sys
from pathlib import Path

ORDER_H = Path(__file__).resolve().parents[1] / "firmware" / "lib" / "stoprock_core" / "src" / "throw_order.h"


def order_constant(pattern, name):
    """ThrowOrder's timing, read from throw_order.h so the analysis checks what the device does."""
    m = re.search(pattern, ORDER_H.read_text())
    if not m:
        sys.exit(f"{name} not found in {ORDER_H}")
    return m.groups()


INTERVAL_S = [int(v) for v in order_constant(r"kIntervalS\[4\]\s*=\s*\{(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\}", "kIntervalS")]
SKIP_SWAP_S = int(order_constant(r"kSkipSwapS\s*=\s*(\d+)", "kSkipSwapS")[0])
END_BREAK_S = int(order_constant(r"kEndBreakS\s*=\s*(\d+)", "kEndBreakS")[0])
SHOT_LAG_S = int(order_constant(r"kShotLagUs\s*=\s*(\d+)LL", "kShotLagUs")[0])
POSITION = {"lead": 0, "second": 1, "third": 2, "skip": 3}
NAMES = {"lead": "Lead", "second": "2nd", "third": "3rd", "skip": "Skip", "them": "THEM", None: "-"}
BUCKETS = [(0, 80, "under 80 s"), (80, 140, "80-140 s"), (140, 200, "140-200 s"), (200, 10 ** 9, "200 s +")]


def games(paths):
    for path in paths:
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line.startswith("{"):
                    continue
                try:
                    g = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if "stones" in g:
                    yield g


def mmss(s):
    return "  -  " if s is None else f"{s // 60:2d}:{s % 60:02d}"


def team(player):
    return None if player is None else ("them" if player == "them" else "us")


def describe(values):
    if not values:
        return "none"
    values = sorted(values)
    q = statistics.quantiles(values, n=4) if len(values) >= 4 else [values[0], statistics.median(values), values[-1]]
    return f"{len(values)} gaps, median {statistics.median(values):.0f} s (middle half {q[0]:.0f}-{q[-1]:.0f} s)"


def full_ends(g):
    """The game's stones as throws: ends of 16 from its first stone, each throw (start, team,
    position). A single untimed throw is filled in (start None) where a team has two stones in a
    row within an end: your own (competition and league can't time you), or anyone's. Two in a row
    across ends is just who threw first, so nothing is filled there. The last end may be short
    (its last stones untimed). Only the ends whose order checks out (alternating, your team's
    stones in order lead, lead, 2nd, 2nd, ...) are returned."""
    seq = [(st[3], team(st[2]), POSITION.get(st[2])) for st in g["stones"] if len(st) >= 4 and st[3] is not None]
    ours = {p for _, t, p in seq if t == "us"}
    me = (set(range(4)) - ours).pop() if len(ours) == 3 else None  # competition and league: you
    ends, end = [], []
    for ev in seq:
        if len(end) == 16:
            ends.append(end)
            end = []
        if end and end[-1][1] == ev[1]:
            # one throw of the other team in between went untimed (ours: you, if it's competition or league)
            missing = ("us", me) if ev[1] == "them" else ("them", None)
            end.append((None, missing[0], missing[1]))
            if len(end) == 16:
                ends.append(end)
                end = []
        end.append(ev)
    if end:
        ends.append(end)  # the last, perhaps short
    order = [0, 0, 1, 1, 2, 2, 3, 3]
    good = []
    for end in ends:
        positions = [p for _, t, p in end if t == "us"]
        # an untimed stone of ours has no position: it takes the one the order says
        if len(positions) <= 8 and all(p is None or p == order[i] for i, p in enumerate(positions)):
            good.append([(st, t, order[[k for k, (_, tt, _) in enumerate(end) if tt == "us"].index(i)] if t == "us" else None)
                         for i, (st, t, _) in enumerate(end)])
    return good


def main(paths):
    if not paths:
        sys.exit(__doc__)
    by_bucket = {label: [0, 0] for *_, label in BUCKETS}
    per_throw = {k: [] for k in ("lead", "2nd", "3rd", "first skip", "skip", "end break")}
    ends_used = 0
    first = [0, 0]
    adjacent, after_shot = [], []
    seen = 0
    for g in games(paths):
        if g.get("v", 0) < 5:
            continue
        seen += 1
        print(f"\nGame {g['id']} ({g['mode']}): {len(g['stones'])} stones, {g.get('thrown', 0)} shots logged")
        ends = full_ends(g)
        ends_used += len(ends)
        flat = [th for end in ends for th in end]  # throw 16 * e + j of the fully timed ends
        for i, (start, _, _) in enumerate(flat):
            if start is None or i == 0:
                continue
            keys = lambda k: ("end break" if k % 16 == 0 else "first skip" if k % 16 == 12 else
                              ("lead", "2nd", "3rd", "skip")[k % 16 // 4])
            if flat[i - 1][0] is not None:
                per_throw[keys(i)].append(start - flat[i - 1][0])
            elif i >= 2 and flat[i - 2][0] is not None:  # around your untimed throw: half each
                half = (start - flat[i - 2][0]) / 2
                per_throw[keys(i - 1)].append(half)
                per_throw[keys(i)].append(half)
        print("    #  start   gap   guess  ended up")
        # your throws (league): taken as 20 s before you logged them. A coach logs the whole team's,
        # timed or not, so in coach they don't stand for throws nobody timed.
        shot_times = (g.get("shot_times") or []) if g.get("mode") == "league" else []
        throws = sorted(t - SHOT_LAG_S for t in shot_times if t is not None)
        prev_start, prev_team = None, None
        for i, stone in enumerate(g["stones"]):
            b2h, h2h, player, start, guess = (stone + [None] * 5)[:5]
            if start is None:
                continue
            # the last event before this stone: the previous stone's start or one of your throws
            mine = [t for t in throws if (prev_start is None or t > prev_start) and t < start]
            if mine:
                after_shot.append(start - (mine[-1] + SHOT_LAG_S))
                anchor, anchor_team = mine[-1], "us"
            else:
                anchor, anchor_team = prev_start, prev_team
            gap = None if anchor is None else start - anchor
            right = guess == player
            mark = "" if right else "   <- corrected"
            print(f"  {i + 1:3d}  {mmss(start)}  {'' if gap is None else f'{gap:4d}':>4}  "
                  f"{NAMES.get(guess, guess):>5}  {NAMES.get(player, player)}{mark}")
            if gap is None:
                first[0] += right
                first[1] += 1
            else:
                for lo, hi, label in BUCKETS:
                    if lo <= gap < hi:
                        by_bucket[label][0] += right
                        by_bucket[label][1] += 1
                if anchor_team and team(player) and anchor_team != team(player) and gap < 150:
                    adjacent.append(gap)
            prev_start, prev_team = start, team(player)
    if not seen:
        print("No games with start times (format version 5) found.")
        return
    print("\nGuesses right, by the gap since the previous stone (or your throw):")
    total = [first[0] + sum(r for r, _ in by_bucket.values()), first[1] + sum(n for _, n in by_bucket.values())]
    if first[1]:
        print(f"  first stone of a game  {first[0]}/{first[1]}")
    for *_, label in BUCKETS:
        r, n = by_bucket[label]
        if n:
            print(f"  {label:22s} {r}/{n} ({100 * r // n}%)")
    if total[1]:
        print(f"  all                    {total[0]}/{total[1]} ({100 * total[0] // total[1]}%)")
    expected = {"lead": INTERVAL_S[0], "2nd": INTERVAL_S[1], "3rd": INTERVAL_S[2],
                "first skip": INTERVAL_S[3] + SKIP_SWAP_S, "skip": INTERVAL_S[3], "end break": INTERVAL_S[0] + END_BREAK_S}
    print(f"\nTime to each stone from the one before, by who threw it ({ends_used} end(s) timed from the start):")
    if not ends_used:
        print("  none: time an end from its first stone, every stone but your own, to see these")
    for key, gaps in per_throw.items():
        if gaps:
            print(f"  {key:11s} {describe(gaps):45s} expected {expected[key]} s")
    print(f"\nA throw apart (team changes, under 150 s): {describe(adjacent)}")
    if after_shot:
        print(f"League, next stone after you logged a shot: {describe(after_shot)}")
        print(f"  assumed: your throw {SHOT_LAG_S} s before you log it")


if __name__ == "__main__":
    main(sys.argv[1:])
