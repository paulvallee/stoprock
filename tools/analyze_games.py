"""Check the throw order's assumptions against real games.

    python tools/analyze_games.py games.txt [more files...]

Reads games exported from the device (the "@GAMES" listing of the `games` command, or plain JSON
lines; other lines are ignored) and, for competition and league games recorded with format
version 5 or later, prints:

- each timed stone: when it started, the gap since the previous stone (or your logged shot, in
  league), who the device guessed, and who it ended up under (a guess that differs is one you
  corrected: move wrong times before exporting, as the final column is taken as the truth);
- how often the guess was right, by gap, against the rules in throw_order.h: under 80 s is the next
  throw, longer gaps count about 56 s a throw;
- the gaps between stones that look a throw apart (the other team's stone straight after ours, or
  ours after theirs, under 150 s), which is what those rules assume;
- league: how long after your logged shot the next stone started (the device takes your throw as
  20 s before you log it).
"""
import json
import statistics
import sys

NEXT_THROW_S, THROW_S, SHOT_LAG_S = 80, 56, 20
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


def main(paths):
    if not paths:
        sys.exit(__doc__)
    by_bucket = {label: [0, 0] for *_, label in BUCKETS}
    first = [0, 0]
    adjacent, after_shot = [], []
    seen = 0
    for g in games(paths):
        if g.get("v", 0) < 5 or g.get("mode") not in ("competition", "league"):
            continue
        seen += 1
        print(f"\nGame {g['id']} ({g['mode']}): {len(g['stones'])} stones, {g.get('thrown', 0)} shots logged")
        print("    #  start   gap   guess  ended up")
        # your throws (league): taken as 20 s before you logged them
        throws = sorted(t - SHOT_LAG_S for t in (g.get("shot_times") or []) if t is not None)
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
        print("No competition or league games with start times (format version 5) found.")
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
    print(f"\nA throw apart (team changes, under 150 s): {describe(adjacent)}")
    print(f"  assumed: under {NEXT_THROW_S} s is the next throw, about {THROW_S} s a throw")
    if after_shot:
        print(f"League, next stone after you logged a shot: {describe(after_shot)}")
        print(f"  assumed: your throw {SHOT_LAG_S} s before you log it, so the next stone ~{THROW_S - SHOT_LAG_S} s after")


if __name__ == "__main__":
    main(sys.argv[1:])
