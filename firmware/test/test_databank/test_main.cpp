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
    TEST_ASSERT_EQUAL_UINT16(0x3, g.shotBits);
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
    TEST_ASSERT_EQUAL_size_t(Databank::kHeaderSize + 8 + 1, n);
    GameRecord back;
    TEST_ASSERT_TRUE(Databank::decode(buf, n, back));
    TEST_ASSERT_EQUAL_UINT32(77, back.id);
    TEST_ASSERT_EQUAL_UINT16(0b0111011, back.shotBits);
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
    r.stones[1] = StoneTimes{NONE, 1402};
    char out[256];
    Databank::toJson(r, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING(
        "{\"id\":12,\"v\":1,\"started\":100,\"updated\":200,\"clock\":true,\"shots\":\"MXM\","
        "\"made\":2,\"thrown\":3,\"pct\":67,\"stones\":[[3.47,13.92],[null,14.02]]}",
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
    RUN_TEST(test_json_line);
    return UNITY_END();
}
