#pragma once

#include <stdint.h>
#include <cstdlib>  // std::abs

namespace Scarfnet
{

// Pure SWARM arrival-delta EMA logic, extracted from Mesh::recordArrivalDelta
// so it can be unit-tested on the native platform without Arduino/mesh deps.
//
// Background: each heartbeat carries the sender's mesh timestamp.  The
// receiver subtracts it from its own clock to get an "arrival delta" — a
// rough estimate of one-way propagation delay.  These are smoothed with an
// EMA and stored per node for future use in swarm-pattern phase offsets.
//
// Bug this guards against: on rejoin after a crash, the receiver's clock can
// be off by seconds to minutes relative to the established-mesh time.  The
// raw delta for that first heartbeat is therefore wildly wrong.  Without
// protection, it poisons the EMA and takes ~80 heartbeats to wash out.

// Alpha for the EMA: each new sample contributes 20% of the updated value.
constexpr float kSwarmEmaAlpha = 0.2f;

// Returns true if rawDeltaMs is within the plausible range (i.e. the sample
// should be fed into the EMA), false if it should be discarded.
//
// The clamp catches the multi-second or multi-minute errors that appear
// during painlessMesh clock convergence after a node reboots or joins fresh.
inline bool swarmDeltaIsPlausible(int32_t rawDeltaMs, int32_t maxDeltaMs)
{
    return std::abs(rawDeltaMs) <= maxDeltaMs;
}

// Apply one EMA step and return the new smoothed value.
// prev should be 0 for a newly-seen node (seed-at-zero policy).
inline int32_t swarmEmaUpdate(int32_t rawDeltaMs, int32_t prev)
{
    return (int32_t)(kSwarmEmaAlpha * rawDeltaMs + (1.0f - kSwarmEmaAlpha) * prev);
}

} // namespace Scarfnet
