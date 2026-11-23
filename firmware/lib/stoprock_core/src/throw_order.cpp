#include "throw_order.h"

namespace stoprock {

void ThrowOrder::reset(uint8_t me) {
    me_ = me % 4;
    last_ = before_ = Pos{};
    lastT_ = -1;
    lastN_ = 1;
    movable_ = false;
}

uint8_t ThrowOrder::columnOf(int s, bool ourFirst) const {
    if ((s % 2 == 0) != ourFirst) return kThem;
    const int p = s / 4;  // our stones 0-7 (s / 2): two each, lead first
    if (p == me_) return kMine;
    return static_cast<uint8_t>(p < me_ ? p : p - 1);
}

int ThrowOrder::throwsSince(int64_t t) const {
    if (t < 0 || lastT_ < 0) return 1;
    const int64_t gap = t - lastT_;
    if (gap < kNextThrowUs) return 1;
    const int64_t n = (gap + kThrowUs / 2) / kThrowUs;
    return n < 2 ? 2 : n > 24 ? 24 : static_cast<int>(n);
}

// The nearest throw to n throws after from that belongs to want, in this end or the next.
// Costs are in quarter throws: a throw's distance from n is 4, changing who throws first within an
// end once it has a throw placed 12 (1 before), going against the guess for a new end 2.
ThrowOrder::Pos ThrowOrder::best(const Pos& from, int n, int want) const {
    Pos pick = from;
    int pickCost = 1 << 30, pickPos = 0;
    for (int e = 0; e < 2; ++e) {
        for (int s = 0; s < 16; ++s) {
            const int pos = e * 16 + s - from.s;
            if (pos < 1) continue;
            for (int f = 0; f < 2; ++f) {
                const bool first = f == 0;
                const uint8_t col = columnOf(s, first);
                if (want < 0 ? col == kMine : col != want) continue;
                int cost = 4 * (pos > n ? pos - n : n - pos);
                if (e == 0 && first != from.ourFirst) cost += from.s >= 0 ? 12 : 1;
                if (e == 1 && first == from.ourFirst) cost += 2;
                if (cost < pickCost || (cost == pickCost && pos < pickPos)) {
                    pickCost = cost;
                    pickPos = pos;
                    pick = Pos{static_cast<int16_t>(from.end + e), static_cast<int8_t>(s), first};
                }
            }
        }
    }
    return pick;
}

uint8_t ThrowOrder::predict(int64_t t) const {
    const Pos p = best(last_, throwsSince(t), -1);
    return columnOf(p.s, p.ourFirst);
}

int ThrowOrder::throwsToMine() const {
    for (int pos = 1; pos <= 32; ++pos) {
        const int i = last_.s + pos;
        const bool next = i >= 16;  // the next end: who throws first swaps, the usual guess
        if (columnOf(i % 16, next ? !last_.ourFirst : last_.ourFirst) == kMine) return pos;
    }
    return -1;
}

int64_t ThrowOrder::nextChange(int64_t t) const {
    if (lastT_ < 0) return -1;
    const int n = throwsSince(t);
    if (n >= 24) return -1;
    // 1 -> 2 at 80 s; then n -> n + 1 halfway between throws: (n + 0.5) throws
    return n == 1 ? lastT_ + kNextThrowUs : lastT_ + (2 * n + 1) * kThrowUs / 2;
}

void ThrowOrder::place(uint8_t c, int64_t t) {
    before_ = last_;
    lastN_ = throwsSince(t);
    last_ = best(last_, lastN_, c);
    lastT_ = t;
    movable_ = true;
}

void ThrowOrder::move(uint8_t c) {
    if (movable_) last_ = best(before_, lastN_, c);
}

void ThrowOrder::ownShot(int e, int r, int64_t t) {
    const int64_t at = t - kShotLagUs;
    const int n = throwsSince(at);
    const int k = 2 * me_ + (r ? 1 : 0);  // your stone of the team's eight
    Pos pick = last_;
    int pickCost = 1 << 30;
    for (int f = 0; f < 2; ++f) {
        const bool first = f == 0;
        const int s = 2 * k + (first ? 0 : 1);
        const int pos = (e - last_.end) * 16 + s - last_.s;
        int cost = 4 * (pos > n ? pos - n : n - pos) + (pos < 1 ? 40 : 0);
        if (e == last_.end && first != last_.ourFirst) cost += last_.s >= 0 ? 12 : 1;
        if (e == last_.end + 1 && first == last_.ourFirst) cost += 2;
        if (cost < pickCost) {
            pickCost = cost;
            pick = Pos{static_cast<int16_t>(e), static_cast<int8_t>(s), first};
        }
    }
    last_ = pick;
    lastT_ = at;
    movable_ = false;
}

}  // namespace stoprock
