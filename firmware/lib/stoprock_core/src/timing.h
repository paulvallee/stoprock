// Timing engine: turns timestamped clicks into back-line-to-hog (B2H) and
// hog-to-hog (H2H) times. Pure logic, no hardware; see docs/SPEC.md section 4.
#pragma once
#include <stdint.h>

namespace stoprock {

struct TimingConfig {
    int64_t doublePressUs = 1000000;  // gaps under this are one click (a heavy tee-to-hog can be ~1.6 s)
    int64_t bhMaxUs = 7000000;        // B2H is [doublePress, bhMax): a stick delivery can take over 5.5 s
    int64_t hhMinUs = 5500000;        // after a B2H, an H2H is at least this (a big-weight one can be ~6 s);
                                      // a click sooner is ignored
    int64_t hhMaxUs = 20000000;       // H2H at most this (H2H-only: [bhMax, hhMax]); also the wait timeout
};

enum class TimingState : uint8_t {
    Idle,        // no stone being timed
    Started,     // first click recorded, waiting for B2H or H2H
    WaitingHH,   // B2H logged, waiting for H2H
};

enum class TimingEvent : uint8_t {
    None,
    Started,       // a click started a new stone
    Restarted,     // double press: timing restarted from the later click
    BH,            // B2H logged (value in Outcome::us); now waiting for H2H
    HH,            // H2H logged; stone done
    IgnoredTooSoon,// click too soon after a B2H; ignored (error buzz)
    Abandoned,     // started but no result within the timeout
    BhClosed,      // B2H logged but no H2H within the timeout; stone done
};

struct Outcome {
    TimingEvent event = TimingEvent::None;
    int64_t us = 0;           // B2H or H2H duration for those events
    bool hadBh = false;       // for H2H: whether this stone already has a B2H
    // Set when a click arrives after a deadline that poll() never processed
    // (for example straight after sleep): the closing event that was missed.
    TimingEvent closedFirst = TimingEvent::None;
};

class TimingEngine {
public:
    explicit TimingEngine(const TimingConfig& cfg = TimingConfig()) : cfg_(cfg) {}

    // A timing click at time t (microseconds, monotonic).
    Outcome click(int64_t t);

    // Check timeouts at time now. Returns Abandoned / BhClosed when they fire.
    Outcome poll(int64_t now);

    TimingState state() const { return state_; }
    bool active() const { return state_ != TimingState::Idle; }

    // Time at which poll() will next produce an event, or -1 if none pending.
    int64_t deadline() const;

    const TimingConfig& config() const { return cfg_; }
    void reset() { state_ = TimingState::Idle; }

private:
    TimingConfig cfg_;
    TimingState state_ = TimingState::Idle;
    int64_t anchor_ = 0;  // start click (Started) or B2H click (WaitingHH)
};

}  // namespace stoprock
