// Shot log for one game: 8 ends x 2 rocks, made or missed, with undo.
#pragma once
#include <stdint.h>

namespace stoprock {

class ShotLog {
public:
    static constexpr uint8_t kSlots = 16;

    // Log a shot at the cursor. Returns false (no change) when all slots are full.
    bool add(bool made);

    // Clear the last logged shot (cursor steps back). False when nothing to undo.
    bool undo();

    uint8_t count() const { return count_; }
    bool made(uint8_t i) const { return (bits_ >> i) & 1u; }
    uint8_t madeCount() const;

    // Slot the next shot will fill, or -1 when full.
    int cursor() const { return count_ < kSlots ? count_ : -1; }

    // Rounded percentage of logged shots that were made, or -1 with none logged.
    int percent() const;

    uint16_t bits() const { return bits_; }
    void restore(uint8_t count, uint16_t bits);
    void clear() { count_ = 0; bits_ = 0; }

private:
    uint8_t count_ = 0;
    uint16_t bits_ = 0;  // bit i set = slot i made
};

}  // namespace stoprock
