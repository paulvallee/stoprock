#include "timing.h"

namespace stoprock {

int64_t TimingEngine::deadline() const {
    if (state_ == TimingState::Idle) return -1;
    return anchor_ + cfg_.hhMaxUs;
}

Outcome TimingEngine::poll(int64_t now) {
    Outcome o;
    if (state_ == TimingState::Idle || now <= anchor_ + cfg_.hhMaxUs) return o;
    o.event = (state_ == TimingState::Started) ? TimingEvent::Abandoned : TimingEvent::BhClosed;
    state_ = TimingState::Idle;
    return o;
}

Outcome TimingEngine::click(int64_t t) {
    // A deadline may have passed without poll() (e.g. the chip was asleep).
    Outcome closed = poll(t);

    Outcome o;
    o.closedFirst = closed.event;
    const int64_t gap = t - anchor_;

    switch (state_) {
    case TimingState::Idle:
        state_ = TimingState::Started;
        anchor_ = t;
        o.event = TimingEvent::Started;
        break;

    case TimingState::Started:
        if (gap < cfg_.doublePressUs) {
            anchor_ = t;
            o.event = TimingEvent::Restarted;
        } else if (gap < cfg_.bhMaxUs) {
            state_ = TimingState::WaitingHH;
            anchor_ = t;
            o.event = TimingEvent::BH;
            o.us = gap;
        } else {  // gap <= hhMax, otherwise poll() above would have closed it
            state_ = TimingState::Idle;
            o.event = TimingEvent::HH;
            o.us = gap;
            o.hadBh = false;
        }
        break;

    case TimingState::WaitingHH:
        if (gap < cfg_.hhMinUs) {
            o.event = TimingEvent::IgnoredTooSoon;
        } else {
            state_ = TimingState::Idle;
            o.event = TimingEvent::HH;
            o.us = gap;
            o.hadBh = true;
        }
        break;
    }
    return o;
}

}  // namespace stoprock
