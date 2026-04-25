#pragma once

// Encode/decode between HeartbeatPacket and raw bytes.
// No hardware deps — can be unit-tested natively.
//
// The wire format is a direct memory copy of the packed struct, so encode is
// a pointer cast and decode is a memcpy + null-terminator guard. The framer
// exists to centralise the length check, the null-terminator, and the CRC-32
// integrity check so every transport (ESP-NOW, Zigbee, …) gets the same validation.
//
// CRC-32 (ISO 3309 / zlib, polynomial 0xEDB88320) is computed over the full
// struct with the crc32 field zeroed. encode() stamps it; decode() verifies it.

#include "heartbeat_packet.h"

#include <cstring>
#include <cstdint>

namespace Scarfnet {

struct HeartbeatFramer {
    // CRC-32 (reflected, poly 0xEDB88320) over `len` bytes.
    static uint32_t crc32(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ (0xEDB88320u & ~((crc & 1u) - 1u));
        }
        return crc ^ 0xFFFFFFFFu;
    }

    // Decode raw bytes into `out`. Returns false if the packet is too short or
    // the CRC-32 does not match. Leaves `out` zeroed on failure.
    static bool decode(const uint8_t* data, int len, HeartbeatPacket& out) {
        if (len < (int)sizeof(HeartbeatPacket)) return false;
        memcpy(&out, data, sizeof(HeartbeatPacket));

        // Verify CRC against the unmodified copy before touching any fields.
        uint32_t received = out.crc32;
        out.crc32 = 0;
        bool ok = (crc32(reinterpret_cast<const uint8_t*>(&out), sizeof(out)) == received);
        if (!ok) { memset(&out, 0, sizeof(out)); return false; }
        out.crc32 = received;

        out.pattern[sizeof(out.pattern) - 1] = '\0';  // guard unterminated data
        return true;
    }

    // Encode `pkt` as raw bytes for transmission. Stamps the CRC-32 into a
    // local copy and sets `len` to the wire size. The returned pointer is valid
    // for the lifetime of `pkt` (the CRC is written into the caller's struct).
    static const uint8_t* encode(HeartbeatPacket& pkt, int& len) {
        pkt.crc32 = 0;
        pkt.crc32 = crc32(reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
        len = (int)sizeof(HeartbeatPacket);
        return reinterpret_cast<const uint8_t*>(&pkt);
    }
};

} // namespace Scarfnet
