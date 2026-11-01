#pragma once

// Wire format for the ESP-NOW heartbeat broadcast.
// Kept in a standalone header with no hardware deps so HeartbeatFramer and
// its tests can compile natively.

#include "mesh_types.h"

#include <cstddef>
#include <cstring>
#include <string>

namespace Scarfnet {

// Packed heartbeat broadcast over ESP-NOW. The layout is the wire format, so
// fields may only be appended; the static_asserts below pin it.
//
// crc32: CRC-32 (ISO 3309 / zlib polynomial) over the full struct with the
// crc32 field zeroed. Computed by HeartbeatFramer::encode; verified and
// rejected by HeartbeatFramer::decode.
//
// `globalRandomizer` is an Rnd (typedefs.h, which needs FastLED, so not used here).
struct __attribute__((packed)) HeartbeatPacket {
    NodeId      id;               // sender node ID (last 4 MAC bytes cast to uint32)
    ScarfNetId  scarfNetId;       // logical network ID; receivers drop mismatched packets
    uint16_t    version;          // sender's kScarfVersion
    uint16_t    tempoIndex;       // version of the tap-tempo state (see tempo_sync.h)
    uint32_t    crc32;            // CRC-32 over full struct with this field zeroed
    TimeMs      currentTimeMs;    // sender's network clock (see clock_sync.h)
    TimeMs      lastPressMs;      // synchronized time of last button press on sender
    NodeId      lastPressId;      // node ID of the scarf that last changed the pattern
    ChangeIndex changeIndex;      // monotonic pattern-change counter
    char        pattern[32];      // null-terminated pattern name
    uint16_t    globalRandomizer; // random seed generated at each pattern change
    uint16_t    beatIntervalMs;   // tap-tempo beat period in ms; 0 = no tap-tempo active
    uint16_t    beatPhaseMs;      // ms elapsed since last beat at time of currentTimeMs
};
static_assert(sizeof(HeartbeatPacket) <= 250, "HeartbeatPacket exceeds the ESP-NOW payload limit");
static_assert(sizeof(HeartbeatPacket) == 70, "wire format changed");
static_assert(offsetof(HeartbeatPacket, version)          == 8,  "wire format changed");
static_assert(offsetof(HeartbeatPacket, tempoIndex)       == 10, "wire format changed");
static_assert(offsetof(HeartbeatPacket, crc32)            == 12, "wire format changed");
static_assert(offsetof(HeartbeatPacket, currentTimeMs)    == 16, "wire format changed");
static_assert(offsetof(HeartbeatPacket, changeIndex)      == 28, "wire format changed");
static_assert(offsetof(HeartbeatPacket, pattern)          == 32, "wire format changed");
static_assert(offsetof(HeartbeatPacket, globalRandomizer) == 64, "wire format changed");
static_assert(offsetof(HeartbeatPacket, beatPhaseMs)      == 68, "wire format changed");

// Copies `name` into pkt.pattern, truncating to 31 chars; always null-terminated.
inline void setPatternName(HeartbeatPacket& pkt, const std::string& name)
{
    size_t len = name.size() < sizeof(pkt.pattern) - 1 ? name.size() : sizeof(pkt.pattern) - 1;
    memcpy(pkt.pattern, name.c_str(), len);
    pkt.pattern[len] = '\0';
}

} // namespace Scarfnet
