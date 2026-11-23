// League: who throws when, so the cursor can be put on the player about to throw.
//
// An end is 16 throws, the two teams alternating. Each team's lead throws their two, then the
// second, third and skip. You time your three teammates and the other team ("THEM"), and log
// your own shots. This keeps track of where in the end the game is (the last throw placed), and:
//   - guesses who throws next: the throw after the last one, skipping yours (you're throwing it);
//   - when a stone starts, uses the time since the previous one to count throws nobody timed: a
//     stone starting under 80 s after the previous one is the next throw; longer gaps are
//     counted at about 56 s a throw (an end of 16 in 15 minutes), at least two;
//   - files each stone where it's told (the column the cursor was on) and moves the game on to the
//     nearest throw of that column, so a correction from the rocker puts it back in step;
//   - takes your logged shots as fixed points: your rock 1 or 2 of the end, in the end your
//     shot log says.
// Who throws first in an end isn't known: the guess is that it swaps every end (the team with
// the last rock usually scores, and the scoring team throws first in the next end), and the first
// stone filed in an end can change it. Within an end it changes only on strong evidence.
#pragma once
#include <stdint.h>

namespace stoprock {

class ThrowOrder {
public:
    static constexpr uint8_t kColumns = 4;   // your three teammates in throwing order, then THEM
    static constexpr uint8_t kThem = 3;      // the other team's column
    static constexpr uint8_t kMine = 0xFE;   // a throw of yours: no column
    static constexpr int64_t kNextThrowUs = 80LL * 1000000;  // under this since the last start: the next throw
    static constexpr int64_t kThrowUs = 56LL * 1000000;      // a throw, typically
    static constexpr int64_t kShotLagUs = 20LL * 1000000;    // you log your shot about this long after throwing

    void reset(uint8_t me);
    // The column (0-3) of throw s (0-15) of an end in which we throw first if ourFirst; kMine for yours.
    uint8_t columnOf(int s, bool ourFirst) const;
    // The column of the next stone, starting at t (microseconds; -1 = not started: the next throw).
    uint8_t predict(int64_t t = -1) const;
    // A stone that started at t was filed in column c.
    void place(uint8_t c, int64_t t);
    // The newest stone was moved to column c: filed again from where the game was before it.
    void move(uint8_t c);
    // You logged your shot: your rock r (0, 1) of end e (0-7), at t.
    void ownShot(int e, int r, int64_t t);
    // Throws from the last one placed to your next (1: you're next), and when the last one started
    // (-1: not known). Your next rock is thrown about (throws - 1) * kThrowUs after the following throw.
    int throwsToMine() const;
    // When the throws counted since the last start (see predict()) next go up, after t: the cursor
    // may move on then. -1: never (nothing placed yet, or far past the end).
    int64_t nextChange(int64_t t) const;
    int64_t lastStart() const { return lastT_; }
    // Your throws so far, up to and including the last throw placed (two an end): compared with
    // the shots logged, how many you still owe.
    int mineSoFar() const;

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
    Pos best(const Pos& from, int n, int want) const;  // want: a column, or -1 = any of the others'

    uint8_t me_ = 0;
    Pos last_, before_;
    int64_t lastT_ = -1;
    int lastN_ = 1;
    bool movable_ = false;  // the newest stone was the last thing placed
};

}  // namespace stoprock
