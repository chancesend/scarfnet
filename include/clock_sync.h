#pragma once

// Network clock estimate from peers' heartbeat timestamps. Hardware-free so it
// is natively testable.
//
// `offset` is the target: the best estimate of network time minus millis().
// timeMs() applies a displayed offset that slews toward the target at up to
// kClockSlewRate, so small corrections play out as a brief speed-up or
// slow-down of the animation rather than a visible skip, and the synced clock
// never runs backward. Hard-sets (warmup, jump to an ahead domain) take effect
// at once.

#include <stdint.h>

#include "config.h"
#include "mesh_types.h"

namespace Scarfnet {

struct ClockSync {
    float offset  = 0.0f;  // target: millis() + (int32_t)offset == network time
    int   samples = 0;     // heartbeats processed so far

    enum class Action { Warmup, Track, JumpAhead, IgnoreBehind };

    // Feed one received peer timestamp. localMs is millis() when the packet
    // is processed. Returns what was done with the sample.
    Action update(TimeMs peerTimeMs, TimeMs localMs) {
        int32_t rawDelta = (int32_t)peerTimeMs - (int32_t)localMs;

        if (samples < kClockWarmupSamples) {
            hardSet((float)rawDelta, localMs);
            ++samples;
            return Action::Warmup;
        }

        // A sample far from our estimate is from a different clock domain —
        // e.g. a group of scarves that rebooted together and warmed up on each
        // other. The domain that is AHEAD wins: jump to a far-ahead sender,
        // ignore a far-behind one (it will jump to ours). Every group converges
        // on the furthest-ahead clock with no voting. We stay warmed up after a
        // jump so a behind-domain peer heard next can't drag us back. Packets
        // are CRC-checked, so a corrupt far-future timestamp is not a concern.
        float diff = (float)rawDelta - offset;
        if (diff > (float)kClockMaxDeviationMs) {
            hardSet((float)rawDelta, localMs);
            return Action::JumpAhead;
        }
        if (diff < -(float)kClockMaxDeviationMs)
            return Action::IgnoreBehind;

        rebaseSlew(localMs);
        offset = kClockEmaAlpha * (float)rawDelta + (1.0f - kClockEmaAlpha) * offset;
        return Action::Track;
    }

    // Offset actually applied at localMs: slewing from where it was at the last
    // update toward the target at kClockSlewRate ms per ms.
    float displayedOffset(TimeMs localMs) const {
        float gap     = offset - _slewFromOffset;
        float maxStep = kClockSlewRate * (float)(localMs - _slewFromMs);
        if (gap >  maxStep) return _slewFromOffset + maxStep;
        if (gap < -maxStep) return _slewFromOffset - maxStep;
        return offset;
    }

    TimeMs timeMs(TimeMs localMs) const {
        return (TimeMs)((int32_t)localMs + (int32_t)displayedOffset(localMs));
    }

    bool isWarmedUp() const { return samples >= kClockWarmupSamples; }

private:
    void hardSet(float newOffset, TimeMs localMs) {
        offset          = newOffset;
        _slewFromOffset = newOffset;
        _slewFromMs     = localMs;
    }

    // Freeze the displayed offset as the new slew start before moving the target.
    void rebaseSlew(TimeMs localMs) {
        _slewFromOffset = displayedOffset(localMs);
        _slewFromMs     = localMs;
    }

    float  _slewFromOffset = 0.0f;
    TimeMs _slewFromMs     = 0;
};

} // namespace Scarfnet
