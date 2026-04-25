#pragma once

// Wire format for the ESP-NOW heartbeat broadcast.
// Kept in a standalone header with no hardware deps so HeartbeatFramer and
// its tests can compile natively.

#include "mesh_types.h"  // NodeId, TimeMs
#include "sync.h"        // ChangeIndex

namespace Scarfnet {

// Packed heartbeat broadcast over ESP-NOW. Well under the 250-byte limit.
// Total: 70 bytes. `pattern` is guaranteed to start at offset 32.
//
//  off   0           4           8    10   12              16          20          24            28          32
//        +-----------+-----------+----+----+---------------+-----------+-----------+-------------+-----------+
//        |    id     | scarfNetId|ver |rsv |     crc32     |currentTime| lastPress | lastPressId |changeIndex|
//        |  (uint32) | (uint32)  |u16 |u16 |   (uint32)    | (uint32)  | (uint32)  |  (uint32)   | (uint32)  |
//        +-----------+-----------+----+----+---------------+-----------+-----------+-------------+-----------+
//           4 B          4 B     2 B  2 B       4 B             4 B        4 B           4 B          4 B
//
//  off   32                              64        66            68          70
//        +-------------------------------+---------+-------------+-----------+
//        |          pattern              |globalRnd| beatInterval|  beatPhase|
//        |          char[32]             | (uint16) |  (uint16)   |  (uint16) |
//        +-------------------------------+---------+-------------+-----------+
//                   32 B                    2 B         2 B          2 B
//
// crc32: CRC-32 (ISO 3309 / zlib polynomial) over the full struct with the
// crc32 field zeroed. Computed by HeartbeatFramer::encode; verified and
// rejected by HeartbeatFramer::decode.
//
// `globalRandomizer` is a random uint16_t generated at each pattern change on the
// initiating scarf. Equivalent to Rnd from typedefs.h, but typedefs.h pulls in
// FastLED which is hardware-only.
struct __attribute__((packed)) HeartbeatPacket {
    NodeId      id;               // sender node ID (last 4 MAC bytes cast to uint32)
    ScarfNetId  scarfNetId;       // logical network ID; receivers drop mismatched packets
    uint16_t    version;          // firmware version (kScarfVersion from config.h)
    uint16_t    _reserved;        // reserved for future use
    uint32_t    crc32;            // CRC-32 over full struct with this field zeroed
    TimeMs      currentTimeMs;    // sender's synchronized clock (millis() + EMA offset)
    TimeMs      lastPressMs;      // synchronized time of last button press on sender
    NodeId      lastPressId;      // node ID of the scarf that last changed the pattern
    ChangeIndex changeIndex;      // monotonic pattern-change counter
    char        pattern[32];      // null-terminated pattern name, max 31 chars; at offset 32
    uint16_t    globalRandomizer; // random seed generated at each pattern change
    uint16_t    beatIntervalMs;   // tap-tempo beat period in ms; 0 = no tap-tempo active
    uint16_t    beatPhaseMs;      // ms elapsed since last beat at time of currentTimeMs
};
static_assert(sizeof(HeartbeatPacket) <= 250, "HeartbeatPacket exceeds ESP-NOW 250-byte limit");
static_assert(offsetof(HeartbeatPacket, pattern) == 32, "pattern must start at offset 32");

} // namespace Scarfnet
