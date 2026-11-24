#include "throw_order.h"

namespace stoprock {

void ThrowOrder::reset(uint8_t me) {
    me_ = me > 3 ? kCoach : me;
    last_ = Pos{};
    lastT_ = -1;
    lastN_ = 1;
    movable_ = false;
    historyCount_ = 0;
}

void ThrowOrder::push() {
    if (historyCount_ == kHistory) {  // full: the oldest step can no longer be undone
        for (int i = 1; i < kHistory; ++i) history_[i - 1] = history_[i];
        --historyCount_;
    }
    history_[historyCount_++] = State{last_, lastT_, lastN_, movable_};
}

bool ThrowOrder::undo() {
    if (historyCount_ == 0) return false;
    const State& h = history_[--historyCount_];
    last_ = h.last;
    lastT_ = h.lastT;
    lastN_ = h.lastN;
    movable_ = h.movable;
    return true;
}

uint8_t ThrowOrder::columnOf(int s, bool ourFirst) const {
    if ((s % 2 == 0) != ourFirst) return themColumn();
    const int p = s / 4;  // our stones 0-7 (s / 2): two each, lead first
    if (p == me_) return kMine;
    return static_cast<uint8_t>(playerColumn(me_, static_cast<uint8_t>(p)));
}

int64_t ThrowOrder::after(int k) const {
    int64_t us = 0;
    for (int i = 1; i <= k; ++i) {
        const int j = last_.s + i;          // may run into the next end
        const int s = ((j % 16) + 16) % 16;
        us += kIntervalS[s / 4] * 1000000LL;
        if (s == 12) us += kSkipSwapS * 1000000LL;
        if (s == 0 && j > 0) us += kEndBreakS * 1000000LL;
    }
    return us;
}

int64_t ThrowOrder::expectedStart(int k) const { return lastT_ < 0 ? -1 : lastT_ + after(k); }

// When the count of throws since the last start goes from n to n + 1: kMissedThrowPct of the way
// from the n-th throw's expected start to the next's.
int64_t ThrowOrder::countUp(int n) const {
    return after(n) + (after(n + 1) - after(n)) * kMissedThrowPct / 100;
}

// The count of throws since the last start, from the time since it: the next one until the gap is
// most of the way (kMissedThrowPct) to the one after, and so on.
int ThrowOrder::throwsSince(int64_t t) const {
    if (t < 0 || lastT_ < 0) return 1;
    const int64_t gap = t - lastT_;
    int n = 1;
    while (n < 24 && gap >= countUp(n)) ++n;
    return n;
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

int ThrowOrder::toLogSoFar() const {
    const bool coach = me_ > 3;
    int n = last_.end * (coach ? 8 : 2);
    for (int s = 0; s <= last_.s; ++s) {
        const uint8_t col = columnOf(s, last_.ourFirst);
        n += coach ? col != themColumn() : col == kMine;
    }
    return n;
}

int64_t ThrowOrder::nextChange(int64_t t) const {
    if (lastT_ < 0) return -1;
    const int n = throwsSince(t);
    if (n >= 24) return -1;
    return lastT_ + countUp(n);
}

void ThrowOrder::place(uint8_t c, int64_t t) {
    push();
    lastN_ = throwsSince(t);
    last_ = best(last_, lastN_, c);
    lastT_ = t;
    movable_ = true;
}

void ThrowOrder::move(uint8_t c) {
    if (movable_ && historyCount_ > 0) last_ = best(history_[historyCount_ - 1].last, lastN_, c);
}

void ThrowOrder::teamShot(int e, int k, int64_t t) {
    push();
    const int64_t at = t - kShotLagUs;
    const int n = throwsSince(at);
    Pos pick = last_;
    int pickCost = 1 << 30;
    for (int f = 0; f < 2; ++f) {
        const bool first = f == 0;
        const int s = 2 * k + (first ? 0 : 1);
        const int pos = (e - last_.end) * 16 + s - last_.s;
        int cost = 4 * (pos > n ? pos - n : n - pos) + (pos < 1 ? 40 : 0);
        // a coach times their own team's stones too: the shot can be the stone just timed
        if (pos == 0 && s == last_.s && first == last_.ourFirst) cost = 4 * (n - 1);
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
