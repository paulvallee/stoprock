// Unit tests for the game databank (run with: pio test -e native)
#include <unity.h>

#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "databank.h"

using namespace stoprock;

// In-memory key-value store standing in for the device's NVS partition.
class MemStore : public KeyValueStore {
public:
    std::map<std::string, std::vector<uint8_t>> data;
    int writes = 0;

    size_t get(const char* key, void* buf, size_t len) override {
        auto it = data.find(key);
        if (it == data.end() || it->second.size() > len) return 0;
        memcpy(buf, it->second.data(), it->second.size());
        return it->second.size();
    }
    bool put(const char* key, const void* d, size_t len) override {
        auto p = static_cast<const uint8_t*>(d);
        data[key] = std::vector<uint8_t>(p, p + len);
        ++writes;
        return true;
    }
    bool remove(const char* key) override { return data.erase(key) > 0; }
};

static constexpr uint16_t NONE = StoneTimes::kNoTime;

void setUp() {}
void tearDown() {}

static void test_empty_game_is_not_stored() {
    MemStore st;
    Databank db(st);
    db.begin();
    db.startGame(1000, true);
    TEST_ASSERT_EQUAL_UINT32(0, db.count());
    TEST_ASSERT_EQUAL_INT(0, st.writes);
    GameRecord g;
    TEST_ASSERT_FALSE(db.lastGame(g));
}

static void test_first_entry_creates_the_record() {
    MemStore st;
    Databank db(st);
    db.begin();
    db.startGame(1000, true);
    db.setShots(1, 0x1, 1010);
    TEST_ASSERT_EQUAL_UINT32(1, db.count());
    TEST_ASSERT_EQUAL_UINT32(1, db.current().id);
    GameRecord g;
    TEST_ASSERT_TRUE(db.load(1, g));
    TEST_ASSERT_EQUAL_UINT8(1, g.shotCount);
    TEST_ASSERT_EQUAL_UINT32(1000, g.started);
    TEST_ASSERT_EQUAL_UINT32(1010, g.updated);
}

static void test_times_are_stored() {
    MemStore st;
    Databank db(st);
    db.begin();
    db.startGame(0, false);
    db.addStone(342, NONE, 1);        // B2H 3.42
    db.setLastHogToHog(1381, 2);      // ... and its H2H 13.81
    db.addStone(NONE, 1402, 3);       // H2H only
    db.addStone(338, NONE, 4);        // B2H only
    GameRecord g;
    TEST_ASSERT_TRUE(db.load(1, g));
    TEST_ASSERT_EQUAL_UINT16(3, g.stoneCount);
    TEST_ASSERT_EQUAL_UINT16(342, g.stones[0].backToHog);
    TEST_ASSERT_EQUAL_UINT16(1381, g.stones[0].hogToHog);
    TEST_ASSERT_EQUAL_UINT16(NONE, g.stones[1].backToHog);
    TEST_ASSERT_EQUAL_UINT16(1402, g.stones[1].hogToHog);
    TEST_ASSERT_EQUAL_UINT16(NONE, g.stones[2].hogToHog);
    TEST_ASSERT_FALSE(g.clockSet);
}

static void test_each_boot_is_a_new_game_and_survives_restart() {
    MemStore st;
    {
        Databank db(st);
        db.begin();
        db.startGame(100, true);
        db.setShots(2, 0x3, 110);
        db.startGame(200, true);            // switched on again
        db.setShots(1, 0x0, 210);
    }
    Databank db(st);                        // reboot: bookkeeping comes back from the store
    db.begin();
    TEST_ASSERT_EQUAL_UINT32(1, db.firstId());
    TEST_ASSERT_EQUAL_UINT32(2, db.lastId());
    GameRecord g;
    TEST_ASSERT_TRUE(db.lastGame(g));
    TEST_ASSERT_EQUAL_UINT32(2, g.id);
    TEST_ASSERT_TRUE(db.load(1, g));
    TEST_ASSERT_TRUE(g.shotBits == 0x3);
    db.startGame(300, true);
    db.addStone(300, NONE, 301);
    TEST_ASSERT_EQUAL_UINT32(3, db.current().id);  // ids carry on
}

static void test_capacity_drops_oldest_and_keeps_ids() {
    MemStore st;
    Databank db(st, 3);
    db.begin();
    for (uint32_t i = 0; i < 5; ++i) {
        db.startGame(i, true);
        db.setShots(1, 1, i);
    }
    TEST_ASSERT_EQUAL_UINT32(3, db.count());
    TEST_ASSERT_EQUAL_UINT32(3, db.firstId());
    TEST_ASSERT_EQUAL_UINT32(5, db.lastId());
    GameRecord g;
    TEST_ASSERT_FALSE(db.load(2, g));
    TEST_ASSERT_TRUE(db.load(3, g));
    TEST_ASSERT_EQUAL_UINT32(2, db.syncedId());  // dropped games can't be synced any more
}

static void test_sync_bookmark() {
    MemStore st;
    Databank db(st);
    db.begin();
    for (uint32_t i = 0; i < 3; ++i) {
        db.startGame(i, true);
        db.setShots(1, 1, i);
    }
    TEST_ASSERT_EQUAL_UINT32(0, db.syncedId());
    db.markSynced(2);
    db.markSynced(1);                      // never goes backwards
    TEST_ASSERT_EQUAL_UINT32(2, db.syncedId());
    db.markSynced(99);                     // capped at the newest game
    TEST_ASSERT_EQUAL_UINT32(3, db.syncedId());
    Databank again(st);
    again.begin();
    TEST_ASSERT_EQUAL_UINT32(3, again.syncedId());
}

static void test_erase_all_keeps_counting_and_current_game() {
    MemStore st;
    Databank db(st);
    db.begin();
    db.startGame(1, true);
    db.setShots(1, 1, 1);
    db.startGame(2, true);
    db.setShots(2, 1, 2);                  // game 2 in progress
    db.eraseAll();
    GameRecord g;
    TEST_ASSERT_FALSE(db.load(1, g));
    TEST_ASSERT_EQUAL_UINT32(3, db.current().id);  // re-stored under a new id
    TEST_ASSERT_TRUE(db.load(3, g));
    TEST_ASSERT_EQUAL_UINT8(2, g.shotCount);
    TEST_ASSERT_EQUAL_UINT32(1, db.count());
}

static void test_encode_decode_round_trip_and_corruption() {
    GameRecord r;
    r.id = 77;
    r.started = 1795290802;
    r.updated = 1795294000;
    r.clockSet = true;
    r.shotCount = 7;
    r.shotBits = 0b0111011;
    r.stoneCount = 2;
    r.stones[0] = StoneTimes{347, 1392};
    r.stones[1] = StoneTimes{NONE, 1402};
    uint8_t buf[Databank::kMaxRecordSize];
    const size_t n = Databank::encode(r, buf);
    TEST_ASSERT_EQUAL_size_t(Databank::kHeaderSize + 2 * Databank::kStoneSize + 1, n);
    GameRecord back;
    TEST_ASSERT_TRUE(Databank::decode(buf, n, back));
    TEST_ASSERT_EQUAL_UINT32(77, back.id);
    TEST_ASSERT_TRUE(back.shotBits == 0b0111011);
    TEST_ASSERT_EQUAL_UINT16(1402, back.stones[1].hogToHog);
    buf[10] ^= 0x40;                                   // flip a bit
    TEST_ASSERT_FALSE(Databank::decode(buf, n, back));
    TEST_ASSERT_FALSE(Databank::decode(buf, n - 1, back));  // truncated
}

static void test_full_game_of_stones() {
    MemStore st;
    Databank db(st);
    db.begin();
    db.startGame(0, true);
    for (int i = 0; i < 130; ++i) db.addStone(300 + i, 1300 + i, i);
    TEST_ASSERT_EQUAL_UINT16(128, db.current().stoneCount);  // extra stones are ignored
    GameRecord g;
    TEST_ASSERT_TRUE(db.load(1, g));
    TEST_ASSERT_EQUAL_UINT16(1427, g.stones[127].hogToHog);
}

static void test_battery_is_logged_per_game() {
    MemStore st;
    Databank db(st);
    db.begin();
    db.startGame(0, true, 90);
    db.setShots(1, 1, 60);
    db.endGame(87, 7200);
    GameRecord g;
    TEST_ASSERT_TRUE(db.load(1, g));
    TEST_ASSERT_EQUAL_UINT8(90, g.batteryStart);
    TEST_ASSERT_EQUAL_UINT8(87, g.batteryEnd);
    TEST_ASSERT_EQUAL_UINT32(7200, g.updated);
    db.startGame(8000, true, 87);
    db.endGame(86, 8100);                  // empty game: nothing stored
    TEST_ASSERT_EQUAL_UINT32(1, db.count());
}

static void test_games_left_estimate() {
    MemStore st;
    Databank db(st);
    db.begin();
    TEST_ASSERT_EQUAL_INT(-1, db.gamesLeft(80));       // nothing to go on yet
    auto game = [&](uint32_t start, uint32_t secs, int b0, int b1) {
        db.startGame(start, true, b0);
        db.setShots(1, 1, start + 1);
        db.endGame(b1, start + secs);
    };
    game(0, 7200, 90, 87);                  // 3 % in a 2-hour game
    TEST_ASSERT_EQUAL_INT(28, db.gamesLeft(87));        // 87 / 3 = 29, less one in reserve
    game(10000, 7200, 87, 82);              // 5 %: average 4 %
    TEST_ASSERT_EQUAL_INT(19, db.gamesLeft(82));        // 82 / 4 = 20.5: 20, less one
    TEST_ASSERT_EQUAL_INT(20, db.gamesLeft(82, 20 * 60, 10, 0));  // no reserve: just rounded down
    game(20000, 300, 82, 70);               // 5-minute game: ignored
    game(30000, 7200, 70, 75);              // charged during the game: ignored
    TEST_ASSERT_EQUAL_INT(17, db.gamesLeft(75));        // 75 / 4 = 18, less one
    TEST_ASSERT_EQUAL_INT(0, db.gamesLeft(7));          // 1.75 games: the last one is the reserve
    TEST_ASSERT_EQUAL_INT(0, db.gamesLeft(3));
    TEST_ASSERT_EQUAL_INT(-1, db.gamesLeft(-1));        // battery unknown
}

// A record as firmware before version 3 wrote it: a 24-byte header, shot bits at 16-17.
static size_t oldRecord(uint8_t version, uint8_t* buf) {
    memset(buf, 0, 24);
    buf[0] = 'G';
    buf[1] = version;
    buf[2] = 1;                             // clock set
    buf[3] = 3;                             // shots
    buf[4] = 5;                             // id 5
    buf[16] = 0b101;                        // shot bits
    buf[18] = 1;                            // one stone
    if (version >= 2) { buf[20] = 50; buf[21] = 40; }
    buf[24] = 0x5B; buf[25] = 0x01;         // B2H 3.47
    buf[26] = 0x70; buf[27] = 0x05;         // H2H 13.92
    const size_t n = 24 + 4 + 1;
    uint8_t c = 0x5A;                       // the checksum, the same way encode() does it
    for (size_t i = 0; i + 1 < n; ++i) c = static_cast<uint8_t>((c << 1 | c >> 7) ^ buf[i]);
    buf[n - 1] = c;
    return n;
}

static void test_reads_version_1_and_2_records() {
    uint8_t buf[Databank::kMaxRecordSize];
    GameRecord back;
    size_t n = oldRecord(1, buf);
    TEST_ASSERT_TRUE(Databank::decode(buf, n, back));
    TEST_ASSERT_EQUAL_UINT32(5, back.id);
    TEST_ASSERT_EQUAL_UINT8(3, back.shotCount);
    TEST_ASSERT_TRUE(back.shotBits == 0b101);
    TEST_ASSERT_EQUAL_UINT16(1392, back.stones[0].hogToHog);
    TEST_ASSERT_EQUAL_UINT8(GameRecord::kNoBattery, back.batteryStart);
    TEST_ASSERT_TRUE(back.mode == GameMode::League);
    n = oldRecord(2, buf);
    TEST_ASSERT_TRUE(Databank::decode(buf, n, back));
    TEST_ASSERT_EQUAL_UINT8(50, back.batteryStart);
    TEST_ASSERT_EQUAL_UINT8(40, back.batteryEnd);
    TEST_ASSERT_TRUE(back.mode == GameMode::League);
    TEST_ASSERT_EQUAL_UINT16(347, back.stones[0].backToHog);
}

static void test_coach_game_of_64_shots_and_mode() {
    MemStore st;
    Databank db(st);
    db.begin();
    db.startGame(10, true, 80, GameMode::Coach);
    const uint64_t bits = 0xF0F0F0F0A5A5A5A5ull;
    db.setShots(64, bits, 20);
    GameRecord g;
    TEST_ASSERT_TRUE(db.load(1, g));
    TEST_ASSERT_TRUE(g.mode == GameMode::Coach);
    TEST_ASSERT_EQUAL_UINT8(64, g.shotCount);
    TEST_ASSERT_TRUE(g.shotBits == bits);
    db.setShots(40, bits, 30);              // undone back to 40: the bits above are dropped
    TEST_ASSERT_TRUE(db.load(1, g));
    TEST_ASSERT_TRUE(g.shotBits == (bits & ((1ull << 40) - 1)));
    db.startGame(100, true, 79, GameMode::Competition);
    db.addStone(347, 1392, 110);
    TEST_ASSERT_TRUE(db.load(2, g));
    TEST_ASSERT_TRUE(g.mode == GameMode::Competition);
    TEST_ASSERT_EQUAL_UINT8(0, g.shotCount);
}

static void test_stone_players_and_version_3() {
    MemStore st;
    Databank db(st);
    db.begin();
    db.startGame(0, true, 80, GameMode::Competition);
    db.addStone(347, StoneTimes::kNoTime, 1, 2);
    db.setLastHogToHog(1392, 2);
    db.addStone(NONE, 1402, 3, 1);
    GameRecord g;
    TEST_ASSERT_TRUE(db.load(1, g));
    TEST_ASSERT_EQUAL_UINT8(2, g.stones[0].player);
    TEST_ASSERT_EQUAL_UINT16(1392, g.stones[0].hogToHog);
    TEST_ASSERT_EQUAL_UINT8(1, g.stones[1].player);
    // a version 3 record: the same header, 4 bytes per stone, no player
    uint8_t buf[Databank::kMaxRecordSize];
    memset(buf, 0, 32);
    buf[0] = 'G';
    buf[1] = 3;
    buf[4] = 9;                             // id 9
    buf[16] = 1;                            // one stone
    buf[18] = buf[19] = 0xFF;               // battery unknown
    buf[20] = 1;                            // competition
    buf[32] = 0x5B; buf[33] = 0x01;         // B2H 3.47
    buf[34] = 0x70; buf[35] = 0x05;         // H2H 13.92
    const size_t n = 32 + 4 + 1;
    uint8_t c = 0x5A;
    for (size_t i = 0; i + 1 < n; ++i) c = static_cast<uint8_t>((c << 1 | c >> 7) ^ buf[i]);
    buf[n - 1] = c;
    TEST_ASSERT_TRUE(Databank::decode(buf, n, g));
    TEST_ASSERT_EQUAL_UINT32(9, g.id);
    TEST_ASSERT_TRUE(g.mode == GameMode::Competition);
    TEST_ASSERT_EQUAL_UINT16(1392, g.stones[0].hogToHog);
    TEST_ASSERT_EQUAL_UINT8(StoneTimes::kNoPlayer, g.stones[0].player);
}

static void test_json_line() {
    GameRecord r;
    r.id = 12;
    r.started = 100;
    r.updated = 200;
    r.clockSet = true;
    r.shotCount = 3;
    r.shotBits = 0b101;
    r.stoneCount = 2;
    r.stones[0] = StoneTimes{347, 1392};
    r.stones[1] = StoneTimes{NONE, 1402, 2};  // thrown by the third
    char out[256];
    Databank::toJson(r, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING(
        "{\"id\":12,\"v\":4,\"mode\":\"league\",\"started\":100,\"updated\":200,\"clock\":true,\"shots\":\"MXM\","
        "\"made\":2,\"thrown\":3,\"pct\":67,\"stones\":[[3.47,13.92,null],[null,14.02,\"third\"]],"
        "\"battery_start\":null,\"battery_end\":null}",
        out);
    char small[20];
    const int need = Databank::toJson(r, small, sizeof small);
    TEST_ASSERT_EQUAL_INT((int)strlen(out), need);   // reports the length it needed
    TEST_ASSERT_EQUAL_size_t(sizeof small - 1, strlen(small));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_game_is_not_stored);
    RUN_TEST(test_first_entry_creates_the_record);
    RUN_TEST(test_times_are_stored);
    RUN_TEST(test_each_boot_is_a_new_game_and_survives_restart);
    RUN_TEST(test_capacity_drops_oldest_and_keeps_ids);
    RUN_TEST(test_sync_bookmark);
    RUN_TEST(test_erase_all_keeps_counting_and_current_game);
    RUN_TEST(test_encode_decode_round_trip_and_corruption);
    RUN_TEST(test_full_game_of_stones);
    RUN_TEST(test_battery_is_logged_per_game);
    RUN_TEST(test_games_left_estimate);
    RUN_TEST(test_reads_version_1_and_2_records);
    RUN_TEST(test_coach_game_of_64_shots_and_mode);
    RUN_TEST(test_stone_players_and_version_3);
    RUN_TEST(test_json_line);
    return UNITY_END();
}
