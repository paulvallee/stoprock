// Unit tests for the timing engine and shot log (run with: pio test -e native)
#include <unity.h>

#include "shots.h"
#include "timing.h"

using namespace stoprock;

static constexpr int64_t S = 1000000;  // one second in microseconds
static int64_t sec(double s) { return static_cast<int64_t>(s * S); }

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- timing
static void test_back_hog_hog_gives_bh_and_hh() {
    TimingEngine t;
    TEST_ASSERT_EQUAL(TimingEvent::Started, t.click(sec(100)).event);
    Outcome bh = t.click(sec(103.2));
    TEST_ASSERT_EQUAL(TimingEvent::BH, bh.event);
    TEST_ASSERT_EQUAL_INT64(sec(3.2), bh.us);
    TEST_ASSERT_EQUAL(TimingState::WaitingHH, t.state());
    Outcome hh = t.click(sec(117.0));
    TEST_ASSERT_EQUAL(TimingEvent::HH, hh.event);
    TEST_ASSERT_EQUAL_INT64(sec(13.8), hh.us);
    TEST_ASSERT_TRUE(hh.hadBh);
    TEST_ASSERT_EQUAL(TimingState::Idle, t.state());
}

static void test_hh_only() {
    TimingEngine t;
    t.click(0);
    Outcome hh = t.click(sec(13.8));
    TEST_ASSERT_EQUAL(TimingEvent::HH, hh.event);
    TEST_ASSERT_FALSE(hh.hadBh);
    TEST_ASSERT_FALSE(t.active());
}

static void test_double_press_restarts() {
    TimingEngine t;
    t.click(0);
    TEST_ASSERT_EQUAL(TimingEvent::Restarted, t.click(sec(0.4)).event);
    Outcome bh = t.click(sec(3.4));
    TEST_ASSERT_EQUAL(TimingEvent::BH, bh.event);
    TEST_ASSERT_EQUAL_INT64(sec(3.0), bh.us);  // measured from the later click
}

static void test_window_boundaries() {
    TimingEngine t;
    t.click(0);
    TEST_ASSERT_EQUAL(TimingEvent::BH, t.click(sec(1.0)).event);  // 1.0 s is B2H
    t.reset();
    t.click(0);
    TEST_ASSERT_EQUAL(TimingEvent::Restarted, t.click(sec(0.99)).event);  // under 1 s: double press
    t.reset();
    t.click(0);
    Outcome heavy = t.click(sec(1.6));                     // big-weight tee-to-hog
    TEST_ASSERT_EQUAL(TimingEvent::BH, heavy.event);
    TEST_ASSERT_EQUAL_INT64(sec(1.6), heavy.us);
    t.reset();
    t.click(0);
    TEST_ASSERT_EQUAL(TimingEvent::HH, t.click(sec(7.0)).event);  // 7 s is H2H
    t.reset();
    t.click(0);
    TEST_ASSERT_EQUAL(TimingEvent::HH, t.click(sec(20.0)).event); // 20 s is still H2H
}

static void test_click_too_soon_after_bh_is_ignored() {
    TimingEngine t;
    t.click(0);
    t.click(sec(3));  // B2H
    TEST_ASSERT_EQUAL(TimingEvent::IgnoredTooSoon, t.click(sec(5)).event);
    TEST_ASSERT_EQUAL(TimingState::WaitingHH, t.state());
    Outcome hh = t.click(sec(17));  // still measured from the B2H click
    TEST_ASSERT_EQUAL(TimingEvent::HH, hh.event);
    TEST_ASSERT_EQUAL_INT64(sec(14), hh.us);
}

static void test_click_after_hh_starts_next_stone_immediately() {
    TimingEngine t;
    t.click(0);
    t.click(sec(3));
    t.click(sec(17));                                            // H2H, done
    TEST_ASSERT_EQUAL(TimingEvent::Started, t.click(sec(19.5)).event);  // 2.5 s later
}

static void test_abandoned_start_times_out() {
    TimingEngine t;
    t.click(0);
    TEST_ASSERT_EQUAL(TimingEvent::None, t.poll(sec(20)).event);
    TEST_ASSERT_EQUAL(TimingEvent::Abandoned, t.poll(sec(20.001)).event);
    TEST_ASSERT_FALSE(t.active());
}

static void test_bh_without_hh_closes_after_timeout() {
    TimingEngine t;
    t.click(0);
    t.click(sec(3));
    TEST_ASSERT_EQUAL_INT64(sec(23), t.deadline());
    TEST_ASSERT_EQUAL(TimingEvent::BhClosed, t.poll(sec(23.5)).event);
    TEST_ASSERT_FALSE(t.active());
}

static void test_late_click_after_missed_deadline_starts_new_stone() {
    TimingEngine t;
    t.click(0);
    t.click(sec(3));                   // B2H; nobody polls (asleep)
    Outcome o = t.click(sec(60));      // much later
    TEST_ASSERT_EQUAL(TimingEvent::Started, o.event);
    TEST_ASSERT_EQUAL(TimingEvent::BhClosed, o.closedFirst);
}

// ---------------------------------------------------------------- shots
static void test_shots_add_undo_percent() {
    ShotLog s;
    TEST_ASSERT_EQUAL_INT(-1, s.percent());
    TEST_ASSERT_EQUAL_INT(0, s.cursor());
    bool seq[] = {true, true, false, true, true, true, false};
    for (bool m : seq) TEST_ASSERT_TRUE(s.add(m));
    TEST_ASSERT_EQUAL_UINT8(7, s.count());
    TEST_ASSERT_EQUAL_UINT8(5, s.madeCount());
    TEST_ASSERT_EQUAL_INT(71, s.percent());
    TEST_ASSERT_EQUAL_INT(7, s.cursor());
    TEST_ASSERT_TRUE(s.undo());
    TEST_ASSERT_EQUAL_INT(6, s.cursor());
    TEST_ASSERT_EQUAL_INT(83, s.percent());
    TEST_ASSERT_TRUE(s.add(true));
    TEST_ASSERT_EQUAL_INT(86, s.percent());
}

static void test_undo_clears_the_slot_bit() {
    ShotLog s;
    s.add(true);
    s.undo();
    s.add(false);
    TEST_ASSERT_FALSE(s.made(0));
    TEST_ASSERT_EQUAL_INT(0, s.percent());
}

static void test_full_and_empty_limits() {
    ShotLog s;
    TEST_ASSERT_FALSE(s.undo());
    for (int i = 0; i < 16; ++i) TEST_ASSERT_TRUE(s.add(true));
    TEST_ASSERT_FALSE(s.add(true));
    TEST_ASSERT_EQUAL_INT(-1, s.cursor());
    TEST_ASSERT_EQUAL_INT(100, s.percent());
}

static void test_percent_rounding() {
    ShotLog s;
    s.add(true); s.add(true); s.add(false);   // 2/3 = 66.7
    TEST_ASSERT_EQUAL_INT(67, s.percent());
    ShotLog h;
    h.add(true); h.add(false);                // 50
    TEST_ASSERT_EQUAL_INT(50, h.percent());
    ShotLog e;
    for (int i = 0; i < 7; ++i) e.add(true);
    e.add(false);                             // 7/8 = 87.5 -> 88
    TEST_ASSERT_EQUAL_INT(88, e.percent());
}

static void test_restore() {
    ShotLog s;
    s.restore(3, 0xFFFF);
    TEST_ASSERT_EQUAL_UINT8(3, s.count());
    TEST_ASSERT_EQUAL_UINT16(0x0007, s.bits());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_back_hog_hog_gives_bh_and_hh);
    RUN_TEST(test_hh_only);
    RUN_TEST(test_double_press_restarts);
    RUN_TEST(test_window_boundaries);
    RUN_TEST(test_click_too_soon_after_bh_is_ignored);
    RUN_TEST(test_click_after_hh_starts_next_stone_immediately);
    RUN_TEST(test_abandoned_start_times_out);
    RUN_TEST(test_bh_without_hh_closes_after_timeout);
    RUN_TEST(test_late_click_after_missed_deadline_starts_new_stone);
    RUN_TEST(test_shots_add_undo_percent);
    RUN_TEST(test_undo_clears_the_slot_bit);
    RUN_TEST(test_full_and_empty_limits);
    RUN_TEST(test_percent_rounding);
    RUN_TEST(test_restore);
    return UNITY_END();
}
