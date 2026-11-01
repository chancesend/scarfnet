#pragma once

// Time-based fade driven by the render clock. Hardware-free so it's natively
// testable.
//
// begin() arms the fade; the start time is captured on the first step() after
// that, so it's measured on the same clock the renderer passes in. Elapsed time
// is unsigned: if the clock steps backward mid-fade, elapsed wraps to a huge
// value and the fade completes immediately rather than stalling.

#include "mesh_types.h"
#include <stdint.h>

namespace Scarfnet {

class Transition
{
public:
    explicit Transition(uint32_t durationMs, bool armed = false)
        : _durationMs(durationMs), _state(armed ? kPending : kIdle) {}

    void begin() { _state = kPending; }

    // Advances the fade at `now`. Returns true while it's in progress and sets
    // `amount` to the blend position (0 = all old, 255 = all new). Returns false
    // once finished (or if never started).
    bool step(TimeMs now, uint8_t& amount)
    {
        if (_state == kPending) {
            _startMs = now;
            _state   = kRunning;
        }
        if (_state != kRunning) return false;

        TimeMs elapsed = now - _startMs;
        if (elapsed >= _durationMs) {
            _state = kIdle;
            return false;
        }
        amount = (uint8_t)(elapsed * 255u / _durationMs);
        return true;
    }

private:
    enum State : uint8_t { kIdle, kPending, kRunning };

    uint32_t _durationMs;
    State    _state;
    TimeMs   _startMs {0};
};

} // namespace Scarfnet
