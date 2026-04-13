#pragma once

#include <stdint.h>

namespace Scarfnet
{

// Pure sync-acceptance logic, extracted from Scarf::onReceivedData so it can
// be unit-tested on the native platform without any Arduino/mesh dependencies.

// Returns true if an incoming changeIndex should replace the local one.
// Rejects equal or older indexes to prevent oscillation and duplicate echoes.
inline bool shouldAcceptUpdate(uint32_t incoming, uint32_t local)
{
    return incoming > local;
}

// Rollover guard: cap values at 0x7fffffff so that a near-overflow node can
// force-reset before the counter wraps and permanently locks out updates.
inline uint32_t rolloverGuard(uint32_t value)
{
    return value > 0x7fffffff ? 0 : value;
}

} // namespace Scarfnet
