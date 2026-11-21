#include "shots.h"

namespace stoprock {

bool ShotLog::add(bool made) {
    if (count_ >= kSlots) return false;
    const uint16_t mask = static_cast<uint16_t>(1u << count_);
    bits_ = made ? (bits_ | mask) : (bits_ & ~mask);
    ++count_;
    return true;
}

bool ShotLog::undo() {
    if (count_ == 0) return false;
    --count_;
    bits_ &= static_cast<uint16_t>(~(1u << count_));
    return true;
}

uint8_t ShotLog::madeCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < count_; ++i) n += made(i);
    return n;
}

int ShotLog::percent() const {
    if (count_ == 0) return -1;
    // round half up, matching Python's round() for these values closely enough
    return (madeCount() * 200 + count_) / (2 * count_);
}

void ShotLog::restore(uint8_t count, uint16_t bits) {
    count_ = count > kSlots ? kSlots : count;
    const uint16_t keep = count_ >= 16 ? 0xFFFFu : static_cast<uint16_t>((1u << count_) - 1);
    bits_ = bits & keep;
}

}  // namespace stoprock
