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
#include <cmath>
#include <cstdlib>

#include "config.h"
#include "mesh_types.h"

namespace Scarfnet {

struct ClockSync {
    TimeDiffMs offset  = 0;  // target: millis() + offset == network time
    int        samples = 0;  // heartbeats processed so far

    enum class Action { Warmup, Track, JumpAhead, IgnoreBehind };

    // Feed one received peer timestamp. localMs is millis() when the packet
    // is processed. Returns what was done with the sample.
    Action update(TimeMs peerTimeMs, TimeMs localMs) {
        TimeDiffMs rawDelta = timeDiff(peerTimeMs, localMs);

        if (samples < kClockWarmupSamples) {
            hardSet(rawDelta, localMs);
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
        TimeDiffMs diff = offsetDiff(rawDelta, offset);
        if (diff > kClockMaxDeviationMs) {
            hardSet(rawDelta, localMs);
            return Action::JumpAhead;
        }
        if (diff < -kClockMaxDeviationMs)
            return Action::IgnoreBehind;

        rebaseSlew(localMs);
        offset = offsetAdd(offset, (TimeDiffMs)lroundf(kClockEmaAlpha * (float)diff));
        return Action::Track;
    }

    // Offset actually applied at localMs: slewing from where it was at the last
    // update toward the target at kClockSlewRate ms per ms.
    TimeDiffMs displayedOffset(TimeMs localMs) const {
        TimeDiffMs gap     = offsetDiff(offset, _slewFromOffset);
        float      maxStep = kClockSlewRate * (float)(localMs - _slewFromMs);
        if (maxStep >= (float)std::abs(gap)) return offset;
        TimeDiffMs step = (TimeDiffMs)maxStep;  // floor: |step| < |gap|, so no overshoot
        return offsetAdd(_slewFromOffset, gap > 0 ? step : -step);
    }

    TimeMs timeMs(TimeMs localMs) const {
        return timeAdd(localMs, displayedOffset(localMs));
    }

    bool isWarmedUp() const { return samples >= kClockWarmupSamples; }

private:
    void hardSet(TimeDiffMs newOffset, TimeMs localMs) {
        offset          = newOffset;
        _slewFromOffset = newOffset;
        _slewFromMs     = localMs;
    }

    // Freeze the displayed offset as the new slew start before moving the target.
    void rebaseSlew(TimeMs localMs) {
        _slewFromOffset = displayedOffset(localMs);
        _slewFromMs     = localMs;
    }

    TimeDiffMs _slewFromOffset = 0;
    TimeMs     _slewFromMs     = 0;
};

} // namespace Scarfnet
