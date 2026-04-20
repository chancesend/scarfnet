#pragma once

// Wire format for the ESP-NOW heartbeat broadcast.
// Kept in a standalone header with no hardware deps so HeartbeatFramer and
// its tests can compile natively.

#include "mesh_types.h"  // NodeId, TimeMs
#include "sync.h"        // ChangeIndex

namespace Scarfnet {

// Packed heartbeat broadcast over ESP-NOW. Well under the 250-byte limit.
// Total: 65 bytes. `pattern` starts at offset 32 (4-byte aligned).
//
//  byte  0       4       8      12      16      20  22  24  26  28          32
//        +-------+-------+-------+-------+-------+---+---+---+---+-----------+
//        |  id   |netId  | press |timeMs |change |rnd|ver|bpm|bph| reserved  |
//        | (u32) | (u32) | (u32) | (u32) | (u32) |u16|u16|u16|u16|  u8[4]    |
//        +-------+-------+-------+-------+-------+---+---+---+---+-----------+
//          4 B     4 B     4 B     4 B     4 B    2 B 2 B 2 B 2 B    4 B
//
//  byte  32                  65                  250
//        +--------------------+--------------------+
//        |     pattern        |      reserved      |
//        |    char[33]        |      185 B         |
//        +--------------------+--------------------+
//              33 B
//
// bpm = beatIntervalMs (beat period in ms; 0 = no tap-tempo)
// bph = beatPhaseMs    (ms into beat at time of pkt.currentTimeMs)
//
// `randomizer` is uint16_t (low 2 bytes of lastPress) — equivalent to Rnd from
// typedefs.h, but typedefs.h pulls in FastLED which is hardware-only.
struct __attribute__((packed)) HeartbeatPacket {
    NodeId      id;             // sender node ID (last 4 MAC bytes cast to uint32)
    ScarfNetId  scarfNetId;     // logical network ID; receivers drop mismatched packets
    TimeMs      lastPress;      // synchronized time of last button press on sender
    TimeMs      currentTimeMs;  // sender's synchronized clock (millis() + EMA offset)
    ChangeIndex changeIndex;    // monotonic pattern-change counter
    uint16_t    randomizer;     // pattern seed (low 2 bytes of lastPress)
    uint16_t    version;        // firmware version (kScarfVersion from config.h)
    uint16_t    beatIntervalMs; // tap-tempo beat period in ms; 0 = no tap-tempo active
    uint16_t    beatPhaseMs;    // ms elapsed since last beat at time of currentTimeMs
    uint8_t     _reserved[4];   // reserved for future fields; zero on send, ignored on receive
    char        pattern[33];    // null-terminated pattern name, max 32 chars; 4-byte aligned at offset 32
};
static_assert(sizeof(HeartbeatPacket) <= 250, "HeartbeatPacket exceeds ESP-NOW 250-byte limit");

} // namespace Scarfnet
