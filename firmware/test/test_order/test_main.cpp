// Unit tests for league's throw order (run with: pio test -e native)
#include <unity.h>

#include "throw_order.h"

using namespace stoprock;

static constexpr int64_t S = 1000000;

void setUp() {}
void tearDown() {}

// Plays ends throw by throw, a throw every `every` seconds. Stones of yours are logged as shots
// (unless skipLog), the others timed and filed where predict() puts them unless `skip` says
// nobody timed it. Returns how many timed stones were predicted wrong.
struct Game {
    ThrowOrder o;
    uint8_t me;
    int64_t t = 0;
    int shots = 0;
    int wrong = 0;
    explicit Game(uint8_t m) : me(m) { o.reset(m); }

    void playEnd(bool ourFirst, int every = 55, uint32_t skip = 0, bool fileAsTruth = false) {
        for (int s = 0; s < 16; ++s, t += every * S) {
            const uint8_t truth = o.columnOf(s, ourFirst);
            if (truth == ThrowOrder::kMine) {
                ++shots;
                o.ownShot((shots - 1) / 2, (shots - 1) % 2, t + 25 * S);
                continue;
            }
            if (skip & (1u << s)) continue;
            const uint8_t guess = o.predict(t);
            if (guess != truth) ++wrong;
            o.place(fileAsTruth ? truth : guess, t);
        }
    }
};

static void test_column_of() {
    ThrowOrder o;
    o.reset(1);  // you're 2nd: columns Lead, 3rd, Skip, THEM
    TEST_ASSERT_EQUAL_UINT8(0, o.columnOf(0, true));
    TEST_ASSERT_EQUAL_UINT8(ThrowOrder::kThem, o.columnOf(1, true));
    TEST_ASSERT_EQUAL_UINT8(0, o.columnOf(2, true));
    TEST_ASSERT_EQUAL_UINT8(ThrowOrder::kMine, o.columnOf(4, true));
    TEST_ASSERT_EQUAL_UINT8(ThrowOrder::kMine, o.columnOf(6, true));
    TEST_ASSERT_EQUAL_UINT8(1, o.columnOf(8, true));
    TEST_ASSERT_EQUAL_UINT8(2, o.columnOf(15, false));
    TEST_ASSERT_EQUAL_UINT8(ThrowOrder::kThem, o.columnOf(0, false));
}

static void test_every_stone_timed_is_predicted() {
    for (uint8_t me = 0; me < 4; ++me) {
        Game g(me);
        // the usual swap each end
        for (int e = 0; e < 8; ++e) g.playEnd(e % 2 == 0);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, g.wrong, "position");
        TEST_ASSERT_EQUAL_INT(7, g.o.end());
    }
}

static void test_untimed_stones_are_counted_by_the_gap() {
    Game g(1);
    // nobody timed throws 3 (THEM) and 10 (the 3rd's): the gaps say so
    g.playEnd(true, 55, (1u << 3) | (1u << 10));
    TEST_ASSERT_EQUAL_INT(0, g.wrong);
}

static void test_quick_stones_are_the_next_throw() {
    Game g(2);
    g.playEnd(true, 40);  // a fast end: 40 s a throw, all under 80 s
    TEST_ASSERT_EQUAL_INT(0, g.wrong);
}

static void test_other_team_first_is_learnt_from_one_correction() {
    Game g(1);
    // they throw first; the first guess (us) is wrong and gets corrected, the rest follow
    g.playEnd(false, 55, 0, true);
    TEST_ASSERT_EQUAL_INT(1, g.wrong);
    TEST_ASSERT_FALSE(g.o.ourFirst());
}

static void test_moving_the_newest_stone_refiles_it() {
    ThrowOrder o;
    o.reset(3);  // skip: columns Lead, 2nd, 3rd, THEM
    o.place(0, 0);          // lead, throw 0
    o.place(ThrowOrder::kThem, 50 * S);
    TEST_ASSERT_EQUAL_UINT8(0, o.predict());  // lead's second rock
    o.place(0, 100 * S);
    o.move(1);              // it was the 2nd (the lead's second rock went untimed)
    TEST_ASSERT_EQUAL_INT(4, o.lastThrow());
    TEST_ASSERT_EQUAL_UINT8(ThrowOrder::kThem, o.predict());
}

static void test_your_shot_pins_the_order() {
    ThrowOrder o;
    o.reset(0);  // lead, throwing first
    o.ownShot(0, 0, 25 * S);
    TEST_ASSERT_EQUAL_INT(0, o.lastThrow());
    TEST_ASSERT_EQUAL_UINT8(ThrowOrder::kThem, o.predict());
    o.place(ThrowOrder::kThem, 55 * S);
    TEST_ASSERT_EQUAL_UINT8(ThrowOrder::kThem, o.predict());  // your second rock is yours: then THEM
    o.ownShot(0, 1, 135 * S);
    TEST_ASSERT_EQUAL_INT(2, o.lastThrow());
    TEST_ASSERT_EQUAL_UINT8(ThrowOrder::kThem, o.predict());
}

static void test_throws_to_mine() {
    ThrowOrder o;
    o.reset(1);  // 2nd, throwing first: your rocks are throws 4 and 6
    TEST_ASSERT_EQUAL_INT(5, o.throwsToMine());
    o.place(0, 0);
    o.place(ThrowOrder::kThem, 55 * S);
    o.place(0, 110 * S);
    TEST_ASSERT_EQUAL_INT(2, o.throwsToMine());  // THEM, then you
    o.place(ThrowOrder::kThem, 165 * S);
    TEST_ASSERT_EQUAL_INT(1, o.throwsToMine());
    TEST_ASSERT_EQUAL_INT64(165 * S, o.lastStart());
    o.ownShot(0, 0, 245 * S);
    TEST_ASSERT_EQUAL_INT(2, o.throwsToMine());
}

static void test_cursor_follows_the_clock() {
    ThrowOrder o;
    o.reset(1);  // 2nd, throwing first; the lead's second rock timed at 0
    o.place(0, -1);
    o.place(0, 0);
    TEST_ASSERT_EQUAL_INT(2, o.lastThrow());
    TEST_ASSERT_EQUAL_UINT8(ThrowOrder::kThem, o.predict(10 * S));
    // halfway to the throw after: THEM's (50 s) and yours (52 s) -> 76 s, then 128 s
    TEST_ASSERT_EQUAL_INT64(76 * S, o.nextChange(10 * S));
    TEST_ASSERT_EQUAL_INT64(128 * S, o.nextChange(76 * S));
    // nothing timed since: THEM, you, THEM, you, THEM, then the 3rd, about 6 throws on
    int64_t t = 0, firstThird = -1;
    while ((t = o.nextChange(t)) >= 0 && t < 600 * S)
        if (firstThird < 0 && o.predict(t) == 1) firstThird = t;
    TEST_ASSERT_EQUAL_INT64(286 * S, firstThird);  // halfway between THEM's last (258 s) and the 3rd (314 s)
}

static void test_mine_so_far() {
    const int64_t S2 = 1000000;
    ThrowOrder o;
    o.reset(0);  // lead, throwing first
    TEST_ASSERT_EQUAL_INT(0, o.mineSoFar());
    o.ownShot(0, 0, 45 * S2);
    TEST_ASSERT_EQUAL_INT(1, o.mineSoFar());
    o.place(ThrowOrder::kThem, 70 * S2);
    TEST_ASSERT_EQUAL_INT(1, o.mineSoFar());
    o.place(ThrowOrder::kThem, 180 * S2);  // their next, after your second rock (not logged yet)
    TEST_ASSERT_EQUAL_INT(3, o.lastThrow());
    TEST_ASSERT_EQUAL_INT(2, o.mineSoFar());  // one shot owed
}

static void test_coach_times_every_stone() {
    // a coach times every stone and logs each of the team's rocks ~25 s after it starts
    ThrowOrder o;
    o.reset(ThrowOrder::kCoach);
    TEST_ASSERT_EQUAL_UINT8(4, o.themColumn());
    int wrong = 0, shots = 0;
    int64_t t = 0;
    for (int e = 0; e < 8; ++e) {
        const bool ourFirst = e % 2 == 1;  // they have the first rock in end 1, then it swaps
        for (int s = 0; s < 16; ++s) {
            // the model's own pace (skips slower, the swap, the end break), give or take 5 s
            t += (ThrowOrder::kIntervalS[s / 4] + (s == 12 ? ThrowOrder::kSkipSwapS : 0) +
                  (s == 0 && e > 0 ? ThrowOrder::kEndBreakS : 0) + (s * 7 + e * 3) % 11 - 5) * S;
            const uint8_t truth = o.columnOf(s, ourFirst);
            if (s % 5 != 2) {  // a few go untimed
                if (o.predict(t) != truth) ++wrong;
                o.place(truth, t);
            }
            if (truth != o.themColumn()) {
                o.teamShot(shots / 8, shots % 8, t + 25 * S);
                ++shots;
            }
        }
    }
    TEST_ASSERT_EQUAL_INT(64, shots);
    TEST_ASSERT_LESS_OR_EQUAL_INT(1, wrong);  // at most the very first stone (who throws first)
}

static void test_undo_steps_back() {
    ThrowOrder o;
    o.reset(0);  // lead, throwing first
    TEST_ASSERT_FALSE(o.undo());              // nothing to undo
    o.ownShot(0, 0, 45 * S);
    o.place(ThrowOrder::kThem, 70 * S);
    o.place(0, 120 * S);                       // a mistake: filed as the 2nd's
    TEST_ASSERT_EQUAL_INT(4, o.lastThrow());
    TEST_ASSERT_TRUE(o.undo());               // that time deleted
    TEST_ASSERT_EQUAL_INT(1, o.lastThrow());
    TEST_ASSERT_EQUAL_INT64(70 * S, o.lastStart());
    TEST_ASSERT_EQUAL_UINT8(ThrowOrder::kThem, o.predict());  // after your 2nd rock: THEM
    TEST_ASSERT_TRUE(o.undo());               // THEM's deleted too
    TEST_ASSERT_TRUE(o.undo());               // and your shot
    TEST_ASSERT_EQUAL_INT(-1, o.lastThrow());
    TEST_ASSERT_FALSE(o.undo());
    // after an undo, moving the newest stone still refiles it from before it
    o.place(ThrowOrder::kThem, 0);
    o.place(0, 50 * S);
    o.undo();
    o.place(0, 60 * S);
    o.move(ThrowOrder::kThem);
    TEST_ASSERT_EQUAL_INT(2, o.lastThrow());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_column_of);
    RUN_TEST(test_every_stone_timed_is_predicted);
    RUN_TEST(test_untimed_stones_are_counted_by_the_gap);
    RUN_TEST(test_quick_stones_are_the_next_throw);
    RUN_TEST(test_other_team_first_is_learnt_from_one_correction);
    RUN_TEST(test_moving_the_newest_stone_refiles_it);
    RUN_TEST(test_your_shot_pins_the_order);
    RUN_TEST(test_throws_to_mine);
    RUN_TEST(test_cursor_follows_the_clock);
    RUN_TEST(test_mine_so_far);
    RUN_TEST(test_coach_times_every_stone);
    RUN_TEST(test_undo_steps_back);
    return UNITY_END();
}
