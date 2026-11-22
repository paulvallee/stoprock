// Unit tests for the screen-side stone list (run with: pio test -e native)
#include <unity.h>

#include "screens.h"

using namespace stoprock;

void setUp() {}
void tearDown() {}

static int byPlayer(const Stone* list, int n, int player) {
    int k = 0;
    for (int i = 0; i < n; ++i) k += list[i].player == player;
    return k;
}

static void test_no_players_drops_oldest() {
    Stone list[25];
    uint8_t n = 0;
    for (int i = 0; i < 40; ++i) addStone(list, n, 25, Stone{i, -1});  // league: no players
    TEST_ASSERT_EQUAL_UINT8(25, n);
    TEST_ASSERT_EQUAL_INT32(39, list[0].bh);   // newest first
    TEST_ASSERT_EQUAL_INT32(15, list[24].bh);  // the 25 newest
}

static void test_player_with_few_stones_keeps_them() {
    Stone list[25];
    uint8_t n = 0;
    for (int i = 0; i < 3; ++i) addStone(list, n, 25, Stone{100 + i, -1, 3});   // three by the skip
    for (int i = 0; i < 60; ++i) addStone(list, n, 25, Stone{i, -1, 1});        // then 60 by the second
    TEST_ASSERT_EQUAL_UINT8(25, n);
    TEST_ASSERT_EQUAL_INT(3, byPlayer(list, n, 3));
    TEST_ASSERT_EQUAL_INT(22, byPlayer(list, n, 1));
    TEST_ASSERT_EQUAL_INT32(59, list[0].bh);
    // still newest first: the skip's stones are the oldest, in order
    TEST_ASSERT_EQUAL_INT32(102, list[22].bh);
    TEST_ASSERT_EQUAL_INT32(101, list[23].bh);
    TEST_ASSERT_EQUAL_INT32(100, list[24].bh);
    for (int i = 0; i < 21; ++i) TEST_ASSERT_EQUAL_INT32(59 - i, list[i].bh);
}

static void test_teammates() {
    TEST_ASSERT_EQUAL_UINT8(1, teammate(0, 0));  // lead times 2nd, 3rd, Skip
    TEST_ASSERT_EQUAL_UINT8(3, teammate(0, 2));
    TEST_ASSERT_EQUAL_UINT8(0, teammate(2, 0));  // third times Lead, 2nd, Skip
    TEST_ASSERT_EQUAL_UINT8(1, teammate(2, 1));
    TEST_ASSERT_EQUAL_UINT8(3, teammate(2, 2));
    TEST_ASSERT_EQUAL_UINT8(2, teammate(3, 2));  // skip times Lead, 2nd, 3rd
}

static void test_every_player_keeps_a_screenful() {
    Stone list[25];
    uint8_t n = 0;
    for (int i = 0; i < 300; ++i) addStone(list, n, 25, Stone{i, -1, static_cast<uint8_t>(i % 7 == 0 ? 2 : i % 11 == 0 ? 3 : 1)});
    for (int p = 1; p < 4; ++p) TEST_ASSERT_TRUE(byPlayer(list, n, p) >= kGridRows);
    for (int i = 1; i < n; ++i) TEST_ASSERT_TRUE(list[i].bh < list[i - 1].bh);  // order kept
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_no_players_drops_oldest);
    RUN_TEST(test_player_with_few_stones_keeps_them);
    RUN_TEST(test_every_player_keeps_a_screenful);
    RUN_TEST(test_teammates);
    return UNITY_END();
}
