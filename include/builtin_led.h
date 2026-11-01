#pragma once

// What the built-in status LED shows, as a pure function of scarf state.
// Hardware-free so the indicator priorities and timings are natively testable.
//
// Priority (highest first):
//   1. Unknown-pattern alarm — rapid red flashes (FlashBurst)
//   2. Tap-tempo mode        — dim green idle glow, bright green on each beat
//   3. Sync blink            — brief flash on a shared mesh-time period;
//                              blue normally, red if a peer runs newer firmware

#include "config.h"
#include "defines.h"
#include "mesh_types.h"
#include <stdint.h>

namespace Scarfnet {

// A fixed-length burst of on/off flashes. Retriggering restarts it. Times are
// whatever monotonic clock the caller uses (local millis(), so a mesh clock
// jump can't stretch or cut the burst short).
class FlashBurst
{
public:
    FlashBurst(uint32_t count, uint32_t halfPeriodMs)
        : _durationMs(count * 2 * halfPeriodMs), _halfPeriodMs(halfPeriodMs) {}

    void trigger(uint32_t now) { _startMs = now; _active = true; }

    bool isActive(uint32_t now)
    {
        if (_active && now - _startMs >= _durationMs) _active = false;
        return _active;
    }

    // True during the "on" half of each flash.
    bool isOn(uint32_t now)
    {
        return isActive(now) && ((now - _startMs) / _halfPeriodMs) % 2 == 0;
    }

private:
    uint32_t _durationMs;
    uint32_t _halfPeriodMs;
    uint32_t _startMs {0};
    bool     _active  {false};
};

struct IndicatorState {
    bool     alarmActive;       // FlashBurst::isActive()
    bool     alarmOn;           // FlashBurst::isOn()
    bool     tapTempoMode;
    bool     tempoActive;       // TapTempo::isActive()
    uint16_t beatIntervalMs;    // TapTempo::beatIntervalMs()
    uint16_t beatPhaseMs;       // TapTempo::beatPhaseMs(now)
    TimeMs   meshTimeMs;
    bool     newerVersionSeen;
};

inline CRGB builtinLedColor(const IndicatorState& s)
{
    if (s.alarmActive)
        return s.alarmOn ? CRGB::Red : CRGB::Black;

    if (s.tapTempoMode) {
        // Bright green pulse at the beat rate (~10% duty) confirms tempo lock.
        if (s.tempoActive && s.beatIntervalMs > 0 && s.beatPhaseMs < s.beatIntervalMs / 10)
            return CRGB::Green;
        return CRGB(0, 5, 0);  // dim idle glow: tap-tempo mode is on
    }

    // Sync blink: 5% duty on a shared period so all scarves pulse together.
    constexpr uint32_t kDutyPercent = 5;
    if ((s.meshTimeMs % kSyncBlinkPeriodMs) * 100 < kSyncBlinkPeriodMs * kDutyPercent)
        return s.newerVersionSeen ? CRGB::Red : CRGB::Blue;
    return CRGB::Black;
}

} // namespace Scarfnet
