#pragma once

// Pure tap-tempo types — no hardware, no Arduino, natively testable.
//
// On the tapping scarf (in tap-tempo mode):
//   tap(meshTime)            — call on every short press
//   reset()                  — call when exiting tap-tempo mode
//
// On receiving scarves:
//   setFromPacket(...)       — call when a heartbeat arrives with beatIntervalMs != 0
//
// Any scarf:
//   beatIntervalMs()         — beat period in ms; 0 = not active
//   beatPhaseMs(now)         — ms elapsed since the last beat (0 = on the beat)
//   isActive()               — true once two taps have established a tempo

#include "mesh_types.h"
#include "beat_info.h"
#include <stdint.h>

namespace Scarfnet {

struct TapTempo {
    // A tap gap longer than this resets the sequence (user stopped tapping).
    static constexpr uint32_t kMaxGapMs  = 3000;
    // Rolling average window for smoothing inter-tap intervals.
    static constexpr int      kSmoothing = 4;

    // Record a tap at `now` (mesh time). Two taps establish a tempo.
    void tap(TimeMs now) {
        if (_hasTapped && (now - _lastTapMs) < kMaxGapMs) {
            uint32_t sample = now - _lastTapMs;
            _intervalMs = (_tapCount == 0)
                ? sample
                : (_intervalMs * (kSmoothing - 1) + sample) / kSmoothing;
            _tapCount++;
            _active = (_tapCount >= 1);  // active after 2nd tap (first gap computed)
        } else {
            // First tap or gap too long — start fresh.
            _intervalMs = 0;
            _tapCount   = 0;
            _active     = false;
        }
        _hasTapped  = true;
        _lastBeatMs = now;
        _lastTapMs  = now;
    }

    // Update beat reference from a received heartbeat.
    // `intervalMs`        — sender's beat period
    // `pktCurrentTimeMs`  — sender's mesh time at packet send
    // `pktBeatPhaseMs`    — ms into the beat the sender was at send time
    void setFromPacket(uint16_t intervalMs, TimeMs pktCurrentTimeMs, uint16_t pktBeatPhaseMs) {
        _intervalMs  = intervalMs;
        _lastBeatMs  = pktCurrentTimeMs - pktBeatPhaseMs;
        _active      = true;
        _tapCount    = kSmoothing;  // mark as fully established
    }

    void reset() {
        _active     = false;
        _hasTapped  = false;
        _lastTapMs  = 0;
        _lastBeatMs = 0;
        _intervalMs = 0;
        _tapCount   = 0;
    }

    bool     isActive()       const { return _active; }

    // Beat period in ms; 0 if not active.
    uint16_t beatIntervalMs() const { return _active ? (uint16_t)_intervalMs : 0; }

    // Milliseconds elapsed since the last beat at time `now`. 0 = on the beat.
    uint16_t beatPhaseMs(TimeMs now) const {
        if (!_active || _intervalMs == 0) return 0;
        uint32_t elapsed = (now >= _lastBeatMs) ? (now - _lastBeatMs) : 0;
        return (uint16_t)(elapsed % _intervalMs);
    }

    uint16_t beatNumber(TimeMs now) const {
        if (!_active || _intervalMs == 0) return 0;
        uint32_t elapsed = (now >= _lastBeatMs) ? (now - _lastBeatMs) : 0;
        return (uint16_t)(elapsed / _intervalMs);
    }

    // Snapshot of beat state at `now`, ready to pass to a pattern render function.
    BeatInfo beatInfo(TimeMs now) const {
        return BeatInfo{ beatIntervalMs(), beatPhaseMs(now), beatNumber(now) };
    }

private:
    bool     _active     = false;
    bool     _hasTapped  = false;  // true after the first tap; guards against t=0 false-positive
    TimeMs   _lastTapMs  = 0;   // mesh time of the most recent tap
    TimeMs   _lastBeatMs = 0;   // mesh time of the most recent beat (= lastTapMs on this scarf)
    uint32_t _intervalMs = 0;   // smoothed beat interval
    int      _tapCount   = 0;   // number of inter-tap gaps measured so far
};

} // namespace Scarfnet
