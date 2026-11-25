// League: who throws when, so the cursor can be put on the player about to throw.
//
// An end is 16 throws, the two teams alternating. Each team's lead throws their two, then the
// second, third and skip. You time your three teammates and the other team ("THEM"), and log
// your own shots. This keeps track of where in the end the game is (the last throw placed), and:
//   - guesses who throws next: the throw after the last one, skipping yours (you're throwing it);
//   - when a stone starts, uses the time since the previous one to count throws nobody timed,
//     from the time each throw takes (kIntervalS: by position, skips slowest, plus the swap before
//     the first skip stone and the break between ends): a throw counts as missed only once the gap
//     is kMissedThrowPct of the way to the throw after it (a slow throw, a skip thinking, is far more
//     common than a missed one, which nearly doubles the gap);
//   - files each stone where it's told (the column the cursor was on) and moves the game on to the
//     nearest throw of that column, so a correction from the rocker puts it back in step;
//   - takes your logged shots as fixed points: your rock 1 or 2 of the end, in the end your
//     shot log says.
// Who throws first in an end isn't known: the guess is that it swaps every end (the team with
// the last rock usually scores, and the scoring team throws first in the next end), and the first
// stone filed in an end can change it. Within an end it changes only on strong evidence.
#pragma once
#include <stdint.h>

#include "seats.h"

namespace stoprock {

class ThrowOrder {
public:
    // Columns are numbered as in seats.h: teammates in throwing order, then THEM.
    static constexpr uint8_t kThem = kLeagueColumns - 1;  // the other team's column for a player
    static constexpr uint8_t kCoach = kCoachSeat;          // reset(kCoach): a coach, every position a column
    static constexpr uint8_t kMine = 0xFE;   // a throw of yours: no column
    // Expected time from one stone's start to the next's, in seconds, by who throws the next one:
    // lead, 2nd, 3rd, skip (both teams). Skips take longest; before the first skip stone of an end
    // the skip and vice swap ends (kSkipSwapS more), and between ends there's a break (kEndBreakS
    // more). About 16.7 minutes an end. From one real end timed in full (23 Nov 2026: lead ~45s,
    // 2nd ~50, 3rd ~52, skips 87-130s, 112s before the first skip stone); the end break is still a
    // guess. tools/analyze_games.py reports these from timed games, and reads these lines.
    static constexpr int kIntervalS[4] = {45, 50, 52, 100};
    static constexpr int kSkipSwapS = 15;
    static constexpr int kEndBreakS = 60;
    // How far, in %, from the next throw's expected start to the one after's, the gap must reach for
    // the next throw to count as missed (untimed). 50 would be halfway.
    static constexpr int kMissedThrowPct = 75;
    static constexpr int64_t kShotLagUs = 20LL * 1000000;    // you log your shot about this long after throwing

    // me: your position (0 lead .. 3 skip), or kCoach.
    void reset(uint8_t me);
    // The other team's column: 3 for a player (after your three teammates), 4 for a coach.
    uint8_t themColumn() const { return stoprock::themColumn(me_); }
    // The column of throw s (0-15) of an end in which we throw first if ourFirst; kMine for yours.
    uint8_t columnOf(int s, bool ourFirst) const;
    // The column of the next stone, starting at t (microseconds; -1 = not started: the next throw).
    uint8_t predict(int64_t t = -1) const;
    // A stone that started at t was filed in column c.
    void place(uint8_t c, int64_t t);
    // The newest stone was moved to column c: filed again from where the game was before it.
    void move(uint8_t c);
    // Undo the last place() or teamShot() (a time or a shot deleted): the game goes back to where it
    // was before it. The last kHistory steps can be undone; false when there's none left.
    bool undo();
    static constexpr int kHistory = 32;
    // You logged your shot: your rock r (0, 1) of end e (0-7), at t.
    void ownShot(int e, int r, int64_t t) { teamShot(e, 2 * me_ + (r ? 1 : 0), t); }
    // A rock of your team's was logged (coach: every one): its stone k (0-7: lead, lead, 2nd, ...)
    // of end e, at t.
    void teamShot(int e, int k, int64_t t);
    // Throws from the last one placed to your next (1: you're next).
    int throwsToMine() const;
    // When the throw k after the last one placed should start (k 0: the last one's start; -1 if
    // that isn't known).
    int64_t expectedStart(int k) const;
    // When the throws counted since the last start (see predict()) next go up, after t: the cursor
    // may move on then. -1: never (nothing placed yet, or far past the end).
    int64_t nextChange(int64_t t) const;
    int64_t lastStart() const { return lastT_; }
    // The throws whose shots are logged, so far, up to and including the last throw placed: yours
    // (two an end), or for a coach the whole team's (eight). Compared with the shots logged, how
    // many are owed.
    int toLogSoFar() const;

    int end() const { return last_.end; }
    int lastThrow() const { return last_.s; }  // -1: none yet this end
    bool ourFirst() const { return last_.ourFirst; }

private:
    struct Pos {
        int16_t end = 0;
        int8_t s = -1;
        bool ourFirst = true;
    };
    int throwsSince(int64_t t) const;
    int64_t after(int k) const;  // expected time from the last throw's start to throw k after it
    int64_t countUp(int n) const;  // when (after the last start) the count goes from n to n + 1
    Pos best(const Pos& from, int n, int want) const;  // want: a column, or -1 = any of the others'

    struct State {
        Pos last;
        int64_t lastT;
        int lastN;
        bool movable;
    };
    void push();  // before a step: what undo() goes back to

    uint8_t me_ = 0;
    Pos last_;
    int64_t lastT_ = -1;
    int lastN_ = 1;
    bool movable_ = false;  // the newest stone was the last thing placed
    State history_[kHistory];
    int historyCount_ = 0;
};

}  // namespace stoprock
