// Shot log for one game, made or missed, with undo.
// League: 8 ends x 2 rocks (your own). Coach: 8 ends x 4 positions x 2 rocks (the whole team).
#pragma once
#include <stdint.h>

namespace stoprock {

class ShotLog {
public:
    static constexpr uint8_t kLeagueSlots = 16;
    static constexpr uint8_t kCoachSlots = 64;
    static constexpr uint8_t kMaxSlots = 64;

    explicit ShotLog(uint8_t capacity = kLeagueSlots) { setCapacity(capacity); }

    // How many rocks the game has slots for (clears the log).
    void setCapacity(uint8_t capacity);
    uint8_t capacity() const { return capacity_; }

    // Log a shot at the cursor. Returns false (no change) when all slots are full.
    bool add(bool made);

    // Clear the last logged shot (cursor steps back). False when nothing to undo.
    bool undo();

    uint8_t count() const { return count_; }
    bool made(uint8_t i) const { return (bits_ >> i) & 1u; }
    uint8_t madeCount() const;

    // Slot the next shot will fill, or -1 when full.
    int cursor() const { return count_ < capacity_ ? count_ : -1; }

    // Rounded percentage of logged shots that were made, or -1 with none logged.
    int percent() const;

    // Coach mode: the same for one position (0 lead, 1 second, 2 third, 3 skip).
    // Slot i belongs to position (i % 8) / 2: each end is L L 2 2 3 3 S S.
    int positionPercent(uint8_t position) const;

    uint64_t bits() const { return bits_; }
    void restore(uint8_t count, uint64_t bits);
    void clear() { count_ = 0; bits_ = 0; }

private:
    uint8_t capacity_ = kLeagueSlots;
    uint8_t count_ = 0;
    uint64_t bits_ = 0;  // bit i set = slot i made
};

// Rounded percentage, made of n; -1 when n is 0. (Round half up, like the mockups.)
inline int roundedPercent(unsigned made, unsigned n) { return n ? static_cast<int>((made * 200 + n) / (2 * n)) : -1; }

}  // namespace stoprock
