#include "databank.h"

#include <stdio.h>
#include <string.h>

namespace stoprock {
namespace {

constexpr uint8_t kMagic = 'G';
constexpr const char* kMetaKey = "meta";
constexpr uint8_t kMetaVersion = 1;

void put16(uint8_t* p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
void put32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = v >> (8 * i); }
uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t get32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint8_t checksum(const uint8_t* p, size_t n) {  // simple, enough to catch a torn or foreign value
    uint8_t c = 0x5A;
    for (size_t i = 0; i < n; ++i) c = static_cast<uint8_t>((c << 1 | c >> 7) ^ p[i]);
    return c;
}

}  // namespace

// Layout, version 2: a 24-byte header, 4 bytes per timed stone, then a checksum byte.
//   0 'G'   1 version   2 flags (bit 0: clock set)   3 shot count
//   4-7 id   8-11 started   12-15 updated   16-17 shot bits   18-19 stone count
//   20 battery % at start   21 battery % at end (0xFF = unknown)   22-23 reserved (0)
// Version 1 is the same with bytes 20-23 all reserved: it reads back with the battery unknown.
//   then per stone: back-to-hog (u16), hog-to-hog (u16), hundredths, 0xFFFF = not timed
//   last byte: checksum of everything before it
size_t Databank::encode(const GameRecord& r, uint8_t* out) {
    const uint16_t n = r.stoneCount > GameRecord::kMaxStones ? GameRecord::kMaxStones : r.stoneCount;
    memset(out, 0, kHeaderSize);
    out[0] = kMagic;
    out[1] = kVersion;
    out[2] = r.clockSet ? 1 : 0;
    out[3] = r.shotCount;
    put32(out + 4, r.id);
    put32(out + 8, r.started);
    put32(out + 12, r.updated);
    put16(out + 16, r.shotBits);
    put16(out + 18, n);
    out[20] = r.batteryStart;
    out[21] = r.batteryEnd;
    uint8_t* p = out + kHeaderSize;
    for (uint16_t i = 0; i < n; ++i, p += 4) {
        put16(p, r.stones[i].backToHog);
        put16(p + 2, r.stones[i].hogToHog);
    }
    *p = checksum(out, p - out);
    return (p - out) + 1;
}

bool Databank::decode(const uint8_t* in, size_t len, GameRecord& r) {
    if (len < kHeaderSize + 1 || in[0] != kMagic || in[1] < 1 || in[1] > kVersion || in[3] > 16) return false;
    const uint16_t n = get16(in + 18);
    if (n > GameRecord::kMaxStones || len != kHeaderSize + 4u * n + 1) return false;
    if (in[len - 1] != checksum(in, len - 1)) return false;
    r = GameRecord();
    r.clockSet = in[2] & 1;
    r.shotCount = in[3];
    r.id = get32(in + 4);
    r.started = get32(in + 8);
    r.updated = get32(in + 12);
    r.shotBits = get16(in + 16) & static_cast<uint16_t>(r.shotCount >= 16 ? 0xFFFF : (1u << r.shotCount) - 1);
    r.stoneCount = n;
    if (in[1] >= 2) {
        r.batteryStart = in[20];
        r.batteryEnd = in[21];
    }
    const uint8_t* p = in + kHeaderSize;
    for (uint16_t i = 0; i < n; ++i, p += 4) {
        r.stones[i].backToHog = get16(p);
        r.stones[i].hogToHog = get16(p + 2);
    }
    return true;
}

static int appendTime(char* out, size_t len, uint16_t t) {
    if (t == StoneTimes::kNoTime) return snprintf(out, len, "null");
    return snprintf(out, len, "%u.%02u", t / 100, t % 100);
}

int Databank::toJson(const GameRecord& r, char* out, size_t len) {
    char shots[17];
    uint8_t made = 0;
    for (uint8_t i = 0; i < r.shotCount; ++i) {
        const bool m = (r.shotBits >> i) & 1;
        shots[i] = m ? 'M' : 'X';
        made += m;
    }
    shots[r.shotCount] = 0;
    const int pct = r.shotCount ? (made * 200 + r.shotCount) / (2 * r.shotCount) : -1;
    size_t n = 0;
    auto room = [&]() { return n < len ? len - n : 0; };
    auto add = [&](int w) { if (w > 0) n += static_cast<size_t>(w); };
    add(snprintf(out, len,
                 "{\"id\":%lu,\"v\":%u,\"started\":%lu,\"updated\":%lu,\"clock\":%s,"
                 "\"shots\":\"%s\",\"made\":%u,\"thrown\":%u,\"pct\":%d,\"stones\":[",
                 static_cast<unsigned long>(r.id), kVersion, static_cast<unsigned long>(r.started),
                 static_cast<unsigned long>(r.updated), r.clockSet ? "true" : "false", shots, made,
                 r.shotCount, pct));
    for (uint16_t i = 0; i < r.stoneCount; ++i) {
        add(snprintf(out + (n < len ? n : len), room(), i ? ",[" : "["));
        add(appendTime(out + (n < len ? n : len), room(), r.stones[i].backToHog));
        add(snprintf(out + (n < len ? n : len), room(), ","));
        add(appendTime(out + (n < len ? n : len), room(), r.stones[i].hogToHog));
        add(snprintf(out + (n < len ? n : len), room(), "]"));
    }
    add(snprintf(out + (n < len ? n : len), room(), "],\"battery_start\":"));
    add(r.batteryStart == GameRecord::kNoBattery ? snprintf(out + (n < len ? n : len), room(), "null")
                                                 : snprintf(out + (n < len ? n : len), room(), "%u", r.batteryStart));
    add(snprintf(out + (n < len ? n : len), room(), ",\"battery_end\":"));
    add(r.batteryEnd == GameRecord::kNoBattery ? snprintf(out + (n < len ? n : len), room(), "null")
                                               : snprintf(out + (n < len ? n : len), room(), "%u", r.batteryEnd));
    add(snprintf(out + (n < len ? n : len), room(), "}"));
    return static_cast<int>(n);
}

void Databank::keyFor(uint32_t id, char* key) { snprintf(key, 16, "g%lu", static_cast<unsigned long>(id)); }

// Meta: version, then first, next, synced (u32 each).
void Databank::begin() {
    uint8_t m[13];
    if (store_.get(kMetaKey, m, sizeof m) == sizeof m && m[0] == kMetaVersion) {
        first_ = get32(m + 1);
        next_ = get32(m + 5);
        synced_ = get32(m + 9);
        if (first_ == 0 || next_ < first_) { first_ = next_ = 1; synced_ = 0; }
    }
}

void Databank::saveMeta() {
    uint8_t m[13];
    m[0] = kMetaVersion;
    put32(m + 1, first_);
    put32(m + 5, next_);
    put32(m + 9, synced_);
    store_.put(kMetaKey, m, sizeof m);
}

static uint8_t toBattery(int pct) {
    return pct < 0 ? GameRecord::kNoBattery : static_cast<uint8_t>(pct > 100 ? 100 : pct);
}

void Databank::startGame(uint32_t now, bool clockSet, int batteryPercent) {
    current_ = GameRecord();
    current_.started = current_.updated = now;
    current_.clockSet = clockSet;
    current_.batteryStart = toBattery(batteryPercent);
    currentStored_ = false;
}

void Databank::setShots(uint8_t count, uint16_t bits, uint32_t now) {
    current_.shotCount = count;
    current_.shotBits = bits;
    current_.updated = now;
    save();
}

void Databank::addStone(uint16_t backToHog, uint16_t hogToHog, uint32_t now) {
    if (current_.stoneCount >= GameRecord::kMaxStones) return;  // a full game is 128 stones
    current_.stones[current_.stoneCount++] = StoneTimes{backToHog, hogToHog};
    current_.updated = now;
    save();
}

void Databank::setLastHogToHog(uint16_t hogToHog, uint32_t now) {
    if (current_.stoneCount == 0) return;
    current_.stones[current_.stoneCount - 1].hogToHog = hogToHog;
    current_.updated = now;
    save();
}

void Databank::endGame(int batteryPercent, uint32_t now) {
    if (!currentStored_) return;  // nothing in it: not worth a record
    current_.batteryEnd = toBattery(batteryPercent);
    current_.updated = now;
    save();
}

int Databank::gamesLeft(int batteryPercent, uint32_t minSeconds, uint8_t recent) {
    if (batteryPercent < 0) return -1;
    static GameRecord g;  // ~540 bytes: keep it off the stack
    uint32_t drained = 0, games = 0;
    for (uint32_t id = next_; id-- > first_ && games < recent;) {
        if (!load(id, g)) continue;
        if (g.batteryStart == GameRecord::kNoBattery || g.batteryEnd == GameRecord::kNoBattery) continue;
        if (g.batteryEnd >= g.batteryStart) continue;            // charging, or too little to measure
        if (g.updated < g.started || g.updated - g.started < minSeconds) continue;
        drained += g.batteryStart - g.batteryEnd;
        ++games;
    }
    if (games == 0) return -1;
    return static_cast<int>(static_cast<uint32_t>(batteryPercent) * games / drained);  // rounds down
}

void Databank::save() {
    if (!currentStored_) {
        if (current_.empty()) return;  // nothing worth keeping yet
        current_.id = next_++;
        currentStored_ = true;
        trim();
        saveMeta();
    }
    static uint8_t buf[kMaxRecordSize];
    char key[16];
    const size_t n = encode(current_, buf);
    keyFor(current_.id, key);
    store_.put(key, buf, n);
}

void Databank::trim() {
    char key[16];
    while (next_ - first_ > capacity_) {
        keyFor(first_, key);
        store_.remove(key);
        ++first_;
    }
    if (synced_ < first_ - 1) synced_ = first_ - 1;
}

bool Databank::load(uint32_t id, GameRecord& out) {
    if (id < first_ || id >= next_) return false;
    static uint8_t buf[kMaxRecordSize];
    char key[16];
    keyFor(id, key);
    const size_t n = store_.get(key, buf, sizeof buf);
    return decode(buf, n, out) && out.id == id;
}

bool Databank::lastGame(GameRecord& out) {
    if (currentStored_) { out = current_; return true; }
    for (uint32_t id = next_; id-- > first_;)
        if (load(id, out) && !out.empty()) return true;
    return false;
}

void Databank::markSynced(uint32_t id) {
    if (id >= next_) id = next_ - 1;
    if (id > synced_) { synced_ = id; saveMeta(); }
}

void Databank::eraseAll() {
    char key[16];
    for (uint32_t id = first_; id < next_; ++id) {
        keyFor(id, key);
        store_.remove(key);
    }
    first_ = next_;
    synced_ = next_ - 1;
    if (currentStored_) {  // the game in progress keeps recording, under a new id
        currentStored_ = false;
        current_.id = 0;
        saveMeta();
        save();
    } else {
        saveMeta();
    }
}

}  // namespace stoprock
