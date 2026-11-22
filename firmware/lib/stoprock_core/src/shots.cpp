#include "shots.h"

namespace stoprock {

static uint64_t lowMask(uint8_t n) { return n >= 64 ? ~0ull : ((1ull << n) - 1); }

void ShotLog::setCapacity(uint8_t capacity) {
    capacity_ = capacity == 0 ? 1 : (capacity > kMaxSlots ? kMaxSlots : capacity);
    clear();
}

bool ShotLog::add(bool made) {
    if (count_ >= capacity_) return false;
    const uint64_t mask = 1ull << count_;
    bits_ = made ? (bits_ | mask) : (bits_ & ~mask);
    ++count_;
    return true;
}

bool ShotLog::undo() {
    if (count_ == 0) return false;
    --count_;
    bits_ &= ~(1ull << count_);
    return true;
}

uint8_t ShotLog::madeCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < count_; ++i) n += made(i);
    return n;
}

int ShotLog::percent() const { return roundedPercent(madeCount(), count_); }

int ShotLog::positionPercent(uint8_t position) const {
    unsigned made = 0, n = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if ((i % 8) / 2 != position) continue;
        ++n;
        made += this->made(i);
    }
    return roundedPercent(made, n);
}

void ShotLog::restore(uint8_t count, uint64_t bits) {
    count_ = count > capacity_ ? capacity_ : count;
    bits_ = bits & lowMask(count_);
}

}  // namespace stoprock
