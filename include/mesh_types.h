#pragma once

// Primitive type aliases shared by Mesh, HeartbeatPacket, NodeTracker, and
// ClockSync. Kept in a standalone header with no hardware deps so any of those
// units can be compiled natively for unit tests.

#include <stdint.h>

namespace Scarfnet {

using NodeId = uint32_t;  // last 4 MAC bytes — stable peer identifier
using TimeMs = uint32_t;  // synchronized millisecond clock value

} // namespace Scarfnet
