// Timing engine: turns timestamped clicks into back-line-to-hog (BH) and
// hog-to-hog (HH) times. Pure logic, no hardware; see docs/SPEC.md section 4.
#pragma once
#include <stdint.h>

namespace stoprock {

struct TimingConfig {
    int64_t doublePressUs = 1500000;  // gaps under this are one click
    int64_t bhMaxUs = 7000000;        // BH is [doublePress, bhMax)
    int64_t hhMaxUs = 20000000;       // HH is [bhMax, hhMax]; also the wait timeout
};

enum class TimingState : uint8_t {
    Idle,        // no stone being timed
    Started,     // first click recorded, waiting for BH or HH
    WaitingHH,   // BH logged, waiting for HH
};

enum class TimingEvent : uint8_t {
    None,
    Started,       // a click started a new stone
    Restarted,     // double press: timing restarted from the later click
    BH,            // BH logged (value in Outcome::us); now waiting for HH
    HH,            // HH logged; stone done
    IgnoredTooSoon,// click too soon after a BH; ignored (error buzz)
    Abandoned,     // started but no result within the timeout
    BhClosed,      // BH logged but no HH within the timeout; stone done
};

struct Outcome {
    TimingEvent event = TimingEvent::None;
    int64_t us = 0;           // BH or HH duration for those events
    bool hadBh = false;       // for HH: whether this stone already has a BH
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
    int64_t anchor_ = 0;  // start click (Started) or BH click (WaitingHH)
};

}  // namespace stoprock
