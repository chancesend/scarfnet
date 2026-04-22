#pragma once

#include <stdint.h>

namespace Scarfnet
{

// Monotonic counter tracking which node's pattern state is authoritative.
// Typed explicitly so call sites are clear about what they're comparing.
using ChangeIndex = uint32_t;

// Pure sync-acceptance logic, extracted from Scarf::onReceivedData so it can
// be unit-tested on the native platform without any Arduino/mesh dependencies.

// Returns true if an incoming changeIndex should replace the local one.
// Rejects equal or older indexes to prevent oscillation and duplicate echoes.
inline bool shouldAcceptUpdate(ChangeIndex incoming, ChangeIndex local)
{
    return incoming > local;
}

// Rollover guard: cap values at 0x7fffffff so that a near-overflow node can
// force-reset before the counter wraps and permanently locks out updates.
inline ChangeIndex rolloverGuard(ChangeIndex value)
{
    return value > 0x7fffffff ? 0 : value;
}

} // namespace Scarfnet
