#pragma once

// Primitive types shared across the mesh and sync units, and wrap-safe clock
// arithmetic. Hardware-free so every unit using them compiles natively.

#include <stdint.h>

namespace Scarfnet {

using NodeId      = uint32_t;  // last 4 MAC bytes — stable peer identifier
using TimeMs      = uint32_t;  // millisecond clock value; wraps every ~49.7 days
using TimeDiffMs  = int32_t;   // signed distance between two TimeMs, modulo 2^32
using ChangeIndex = uint32_t;  // shared pattern-change counter; compared wrap-safely
using ScarfNetId  = uint32_t;  // logical network identifier; scarves drop packets from other IDs

// Clock arithmetic is modulo 2^32, so it stays exact at any uptime and across
// the wrap. Plain signed subtraction would overflow (undefined behaviour).
inline TimeDiffMs timeDiff(TimeMs a, TimeMs b)           { return (TimeDiffMs)(a - b); }
inline TimeMs     timeAdd(TimeMs t, TimeDiffMs d)        { return t + (uint32_t)d; }
inline TimeDiffMs offsetDiff(TimeDiffMs a, TimeDiffMs b) { return (TimeDiffMs)((uint32_t)a - (uint32_t)b); }
inline TimeDiffMs offsetAdd(TimeDiffMs a, TimeDiffMs b)  { return (TimeDiffMs)((uint32_t)a + (uint32_t)b); }

} // namespace Scarfnet
