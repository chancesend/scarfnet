#pragma once

// Pure clock-sync logic, extracted from Mesh::updateClock / Mesh::timeMs so it
// can be unit-tested on the native platform without any Arduino or ESP-NOW deps.
//
// Usage:
//   ClockSync cs;
//   cs.update(pktCurrentTimeMs, millis());  // call on each received heartbeat
//   uint32_t now = cs.timeMs(millis());     // synced clock for rendering
//
// Two offsets: `offset` is the target — the best estimate of the network
// clock, updated by the rules in update(). timeMs() returns a displayed offset
// that slews toward the target at up to kClockSlewRate, so small corrections
// (≤ kSwarmMaxClockDeviationMs) play out as a brief speed-up or slow-down of
// the animation rather than a visible skip, and the synced clock never runs
// backward. Hard-sets (warmup, jump to an ahead domain) take effect at once.

#include <stdint.h>
#include <cmath>

#include "config.h"     // kClockWarmupSamples, kSwarmMaxClockDeviationMs, kClockSlewRate
#include "mesh_types.h" // TimeMs
#include "swarm_ema.h"  // kSwarmEmaAlpha

namespace Scarfnet {

struct ClockSync {
    float offset  = 0.0f;  // target: millis() + (int32_t)offset == network time
    int   samples = 0;     // number of heartbeats processed so far

    // Feed one received peer timestamp. localMs should be millis() at the
    // moment the packet was processed (not when it was received by the ISR).
    void update(TimeMs peerTimeMs, TimeMs localMs) {
        int32_t rawDelta = (int32_t)peerTimeMs - (int32_t)localMs;

        if (samples < kClockWarmupSamples) {
            // Hard-set during warmup: a freshly-booted node has millis()≈0
            // while established peers are at hours-long timestamps. The EMA
            // window is too narrow to converge from that distance; hard-set
            // instead so the first few heartbeats snap the offset immediately.
            hardSet((float)rawDelta, localMs);
            ++samples;
            return;
        }

        // After warmup, a sample far from our estimate means the sender is in
        // a different clock domain — e.g. a group of scarves that rebooted
        // together and warmed up on each other. The domain that is AHEAD wins:
        // if the sender is far ahead, hard-set to its clock; if it's far
        // behind, ignore it (it will jump to ours). Every group converges on
        // the furthest-ahead clock with no voting. We stay warmed up after a
        // jump so a behind-domain peer heard next can't drag us back. Packets
        // are CRC-checked, so a corrupt far-future timestamp is not a concern.
        float diff = (float)rawDelta - offset;
        if (diff > (float)kSwarmMaxClockDeviationMs) {
            hardSet((float)rawDelta, localMs);
            return;
        }
        if (diff < -(float)kSwarmMaxClockDeviationMs) return;

        // Small correction: move the target; the displayed clock slews to it.
        rebaseSlew(localMs);
        offset = kSwarmEmaAlpha * (float)rawDelta + (1.0f - kSwarmEmaAlpha) * offset;
    }

    // What update() would do with this sample, for logging.
    enum class Action { Warmup, Track, JumpAhead, IgnoreBehind };
    Action classify(TimeMs peerTimeMs, TimeMs localMs) const {
        if (samples < kClockWarmupSamples) return Action::Warmup;
        float diff = (float)((int32_t)peerTimeMs - (int32_t)localMs) - offset;
        if (diff >  (float)kSwarmMaxClockDeviationMs) return Action::JumpAhead;
        if (diff < -(float)kSwarmMaxClockDeviationMs) return Action::IgnoreBehind;
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

    // Synchronized time given the current local millis() value.
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
