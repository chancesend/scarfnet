#pragma once

// Pure clock-sync logic, extracted from Mesh::updateClock / Mesh::timeMs so it
// can be unit-tested on the native platform without any Arduino or ESP-NOW deps.
//
// Usage:
//   ClockSync cs;
//   cs.update(pktCurrentTimeMs, millis());  // call on each received heartbeat
//   uint32_t now = cs.timeMs(millis());     // synced clock for rendering

#include <stdint.h>
#include <cmath>

#include "config.h"     // kClockWarmupSamples, kSwarmMaxClockDeviationMs
#include "mesh_types.h" // TimeMs
#include "swarm_ema.h"  // kSwarmEmaAlpha

namespace Scarfnet {

struct ClockSync {
    float offset  = 0.0f;  // millis() + (int32_t)offset == synced time
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
            offset = (float)rawDelta;
            ++samples;
            return;
        }

        // After warmup, discard samples that deviate too far from our current
        // estimate. Guards against corrupted packets and adversarial senders.
        float deviation = fabsf((float)rawDelta - offset);
        if (deviation > (float)kSwarmMaxClockDeviationMs) return;

        offset = kSwarmEmaAlpha * (float)rawDelta + (1.0f - kSwarmEmaAlpha) * offset;
    }

    // Synchronized time given the current local millis() value.
    TimeMs timeMs(TimeMs localMs) const {
        return (TimeMs)((int32_t)localMs + (int32_t)offset);
    }

    bool isWarmedUp() const { return samples >= kClockWarmupSamples; }
};

} // namespace Scarfnet
