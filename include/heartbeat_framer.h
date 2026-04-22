#pragma once

// Encode/decode between HeartbeatPacket and raw bytes.
// No hardware deps — can be unit-tested natively.
//
// The wire format is a direct memory copy of the packed struct, so encode is
// a pointer cast and decode is a memcpy + null-terminator guard. The framer
// exists to centralise the length check and the null-terminator so every
// transport (ESP-NOW, Zigbee, …) gets the same validation.

#include "heartbeat_packet.h"

#include <cstring>
#include <cstdint>

namespace Scarfnet {

struct HeartbeatFramer {
    // Decode raw bytes into `out`. Returns false and leaves `out` zeroed if
    // `len` is too short to contain a full HeartbeatPacket.
    static bool decode(const uint8_t* data, int len, HeartbeatPacket& out) {
        if (len < (int)sizeof(HeartbeatPacket)) return false;
        memcpy(&out, data, sizeof(HeartbeatPacket));
        out.pattern[sizeof(out.pattern) - 1] = '\0';  // guard unterminated data
        return true;
    }

    // Encode `pkt` as raw bytes for transmission. Sets `len` to the wire size.
    // The returned pointer is valid for the lifetime of `pkt`.
    static const uint8_t* encode(const HeartbeatPacket& pkt, int& len) {
        len = (int)sizeof(HeartbeatPacket);
        return reinterpret_cast<const uint8_t*>(&pkt);
    }
};

} // namespace Scarfnet
