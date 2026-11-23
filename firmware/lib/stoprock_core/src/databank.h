// Game databank: every game the device has recorded, kept in flash for a companion app.
// Pure logic over a small key-value interface, so it runs (and is tested) off the device.
// See docs/DATABANK.md for the record format and the sync protocol.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace stoprock {

// Minimal key-value storage. On the device: an NVS partition of its own. In tests: memory.
class KeyValueStore {
public:
    virtual ~KeyValueStore() = default;
    // Reads up to len bytes; returns the stored size, or 0 if the key is absent.
    virtual size_t get(const char* key, void* buf, size_t len) = 0;
    virtual bool put(const char* key, const void* data, size_t len) = 0;
    virtual bool remove(const char* key) = 0;
};

// One timed stone, in hundredths of a second. kNoTime = that split wasn't timed.
struct StoneTimes {
    static constexpr uint16_t kNoTime = 0xFFFF;
    uint16_t backToHog = kNoTime;
    uint16_t hogToHog = kNoTime;
    static constexpr uint8_t kNoPlayer = 0xFF;
    static constexpr uint8_t kThem = 4;  // league: a stone of the other team's
    uint8_t player = kNoPlayer;  // competition and league: who threw it, 0 lead .. 3 skip, kThem; kNoPlayer in coach
};

// How the device was switched on (see docs/SPEC.md). 0 = league, so older records read as league.
enum class GameMode : uint8_t { League = 0, Competition = 1, Coach = 2 };
const char* modeName(GameMode m);  // "league", "competition", "coach"

struct GameRecord {
    static constexpr uint16_t kMaxStones = 128;  // 8 ends x 16 stones

    uint32_t id = 0;          // 1, 2, 3, ... never reused, even after old games are dropped
    uint32_t started = 0;     // clock seconds when the game started (see clockSet)
    uint32_t updated = 0;     // clock seconds of the last change
    bool clockSet = false;    // false: the clock hadn't been set, so the times are only relative
    GameMode mode = GameMode::League;
    uint8_t shotCount = 0;    // rocks logged: 0-16 in league, 0-64 in coach, 0 in competition
    uint64_t shotBits = 0;    // bit i set = rock i made
    uint16_t stoneCount = 0;  // timed stones, oldest first
    static constexpr uint8_t kNoBattery = 0xFF;
    uint8_t batteryStart = kNoBattery;  // charge % at switch-on
    uint8_t batteryEnd = kNoBattery;    // charge % at switch-off (unknown if it never switched off cleanly)
    StoneTimes stones[kMaxStones];

    bool empty() const { return shotCount == 0 && stoneCount == 0; }
};

class Databank {
public:
    static constexpr uint8_t kVersion = 4;            // record format version (1-3 still readable)
    static constexpr size_t kHeaderSize = 32;         // versions 3 and 4
    static constexpr size_t kOldHeaderSize = 24;      // versions 1 and 2
    static constexpr size_t kStoneSize = 5;           // version 4; 4 before (no player)
    static constexpr size_t kMaxRecordSize = kHeaderSize + kStoneSize * GameRecord::kMaxStones + 1;
    static constexpr uint32_t kDefaultCapacity = 500;

    explicit Databank(KeyValueStore& store, uint32_t capacity = kDefaultCapacity)
        : store_(store), capacity_(capacity) {}

    // Load the bookkeeping. Call once at start-up.
    void begin();

    // Start a new game (every switch-on). It is only written to flash once it has
    // something in it, so switching on and off doesn't fill the databank with empty games.
    void startGame(uint32_t now, bool clockSet, int batteryPercent = -1, GameMode mode = GameMode::League);

    // The game in progress. Change it through the calls below.
    const GameRecord& current() const { return current_; }
    void setShots(uint8_t count, uint64_t bits, uint32_t now);
    // A stone's first result: a B2H (hogToHog = kNoTime), or an H2H with no B2H.
    void addStone(uint16_t backToHog, uint16_t hogToHog, uint32_t now, uint8_t player = StoneTimes::kNoPlayer);
    // The newest stone was filed under the wrong player (league: moved with the rocker).
    void setLastPlayer(uint8_t player, uint32_t now);
    // The H2H that completes the last stone's B2H.
    void setLastHogToHog(uint16_t hogToHog, uint32_t now);
    // The game ends (switch-off): note the battery. Kept only if the game is stored.
    void endGame(int batteryPercent, uint32_t now);

    // Battery: games left at batteryPercent, from the average drain of the last `recent`
    // completed games (both readings, a drop of at least 1 %, and at least minSeconds long),
    // rounded down, less `reserve` games kept back so the estimate doesn't promise a game the
    // battery can't finish (the charge reading is only an estimate, least reliable near empty).
    // Never below 0; -1 if there are none to go on yet.
    static constexpr uint8_t kReserveGames = 1;
    int gamesLeft(int batteryPercent, uint32_t minSeconds = 20 * 60, uint8_t recent = 10,
                  uint8_t reserve = kReserveGames);

    // Stored games: ids run from firstId() to lastId(); a gap means that record was lost.
    uint32_t firstId() const { return first_; }
    uint32_t lastId() const { return next_ - 1; }
    uint32_t count() const { return next_ - first_; }
    bool load(uint32_t id, GameRecord& out);

    // The most recent stored game with something in it (for the help page). False if none.
    bool lastGame(GameRecord& out);

    // Sync: the app reads games after syncedId(), then acknowledges the last one it kept.
    uint32_t syncedId() const { return synced_; }
    void markSynced(uint32_t id);

    // Drop every stored game. Ids keep counting up, so an app never mistakes a new game for an old one.
    void eraseAll();

    // Record encoding (little-endian, versioned) and a JSON line for export.
    static size_t encode(const GameRecord& r, uint8_t* out);           // up to kMaxRecordSize bytes
    static bool decode(const uint8_t* in, size_t len, GameRecord& r);   // false if not a valid record
    static int toJson(const GameRecord& r, char* out, size_t len);      // snprintf-style length

private:
    void save();             // writes the current game (creating its record if needed)
    void saveMeta();
    void trim();             // keep at most capacity_ games
    static void keyFor(uint32_t id, char* key);

    KeyValueStore& store_;
    uint32_t capacity_;
    uint32_t first_ = 1;     // oldest stored id
    uint32_t next_ = 1;      // id the next stored game gets
    uint32_t synced_ = 0;    // highest id the app has acknowledged
    GameRecord current_;
    bool currentStored_ = false;
};

}  // namespace stoprock
