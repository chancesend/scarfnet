#pragma once

// Wire format for the ESP-NOW heartbeat broadcast.
// Kept in a standalone header with no hardware deps so HeartbeatFramer and
// its tests can compile natively.

#include "mesh_types.h"  // NodeId, TimeMs
#include "sync.h"        // ChangeIndex

namespace Scarfnet {

// Packed heartbeat broadcast over ESP-NOW. Well under the 250-byte limit.
// Current size: 4+4+4+4+1+33 = 50 bytes.
//
// `randomizer` is uint8_t (low byte of lastPress) — equivalent to Rnd from
// typedefs.h, but typedefs.h pulls in FastLED which is hardware-only.
struct __attribute__((packed)) HeartbeatPacket {
    NodeId      id;             // sender node ID (last 4 MAC bytes cast to uint32)
    TimeMs      lastPress;      // synchronized time of last button press on sender
    TimeMs      currentTimeMs;  // sender's synchronized clock (millis() + EMA offset)
    ChangeIndex changeIndex;    // monotonic pattern-change counter
    uint8_t     randomizer;     // pattern seed (low byte of lastPress)
    char        pattern[33];    // null-terminated pattern name, max 32 chars
};
static_assert(sizeof(HeartbeatPacket) <= 250, "HeartbeatPacket exceeds ESP-NOW 250-byte limit");

} // namespace Scarfnet
