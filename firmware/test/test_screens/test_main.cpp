// Unit tests for the screen-side stone list (run with: pio test -e native)
#include <unity.h>

#include "screens.h"

using namespace stoprock;

void setUp() {}
void tearDown() {}

static int inColumn(const Stone* list, int n, int col) {
    int k = 0;
    for (int i = 0; i < n; ++i) k += list[i].col == col;
    return k;
}

static void test_single_column_drops_oldest() {
    Stone list[25];
    uint8_t n = 0;
    for (int i = 0; i < 40; ++i) addStone(list, n, 25, Stone{i, -1, 0});
    TEST_ASSERT_EQUAL_UINT8(25, n);
    TEST_ASSERT_EQUAL_INT32(39, list[0].bh);   // newest first
    TEST_ASSERT_EQUAL_INT32(15, list[24].bh);  // the 25 newest
}

static void test_rare_column_keeps_its_times() {
    Stone list[25];
    uint8_t n = 0;
    for (int i = 0; i < 3; ++i) addStone(list, n, 25, Stone{100 + i, -1, 2});   // three in column 3
    for (int i = 0; i < 60; ++i) addStone(list, n, 25, Stone{i, -1, 0});        // then 60 in column 1
    TEST_ASSERT_EQUAL_UINT8(25, n);
    TEST_ASSERT_EQUAL_INT(3, inColumn(list, n, 2));
    TEST_ASSERT_EQUAL_INT(22, inColumn(list, n, 0));
    TEST_ASSERT_EQUAL_INT32(59, list[0].bh);
    // still newest first: the column-3 stones are the oldest, in order
    TEST_ASSERT_EQUAL_INT32(102, list[22].bh);
    TEST_ASSERT_EQUAL_INT32(101, list[23].bh);
    TEST_ASSERT_EQUAL_INT32(100, list[24].bh);
    for (int i = 0; i < 21; ++i) TEST_ASSERT_EQUAL_INT32(59 - i, list[i].bh);
}

static void test_every_column_keeps_a_screenful() {
    Stone list[25];
    uint8_t n = 0;
    for (int i = 0; i < 300; ++i) addStone(list, n, 25, Stone{i, -1, static_cast<uint8_t>(i % 7 == 0 ? 1 : i % 11 == 0 ? 2 : 0)});
    for (int c = 0; c < 3; ++c) TEST_ASSERT_TRUE(inColumn(list, n, c) >= kGridRows);
    for (int i = 1; i < n; ++i) TEST_ASSERT_TRUE(list[i].bh < list[i - 1].bh);  // order kept
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_single_column_drops_oldest);
    RUN_TEST(test_rare_column_keeps_its_times);
    RUN_TEST(test_every_column_keeps_a_screenful);
    return UNITY_END();
}
