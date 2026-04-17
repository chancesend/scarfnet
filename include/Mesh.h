#pragma once

// defines.h sets SCARFNET_EMBEDDED for the embedded target; include it first
// so the guard below works regardless of include order in the caller.
#include "defines.h"

#ifdef SCARFNET_EMBEDDED

#include <esp_now.h>
#include <freertos/portmacro.h>
#include <functional>
#include <unordered_map>
#include <cstdint>

#include "log.h"
#include "sync.h"  // for ChangeIndex

namespace Scarfnet {

// ── Namespace-level type aliases ─────────────────────────────────────────────
// Defined here so HeartbeatPacket (which lives outside the Mesh class) can use
// them. Mesh re-exports these as class aliases so Mesh::TimeMs / Mesh::NodeId
// also work.
using NodeId = uint32_t;  // last 4 MAC bytes as a stable peer identifier
using TimeMs = uint32_t;  // synchronized millisecond clock value
// ChangeIndex is defined in sync.h (same namespace)

// ── Wire format ──────────────────────────────────────────────────────────────

// Packed heartbeat broadcast over ESP-NOW. Well under the 250-byte limit.
// Current size: 4+4+4+4+1+33 = 50 bytes.
struct __attribute__((packed)) HeartbeatPacket {
    NodeId      id;             // sender node ID (last 4 MAC bytes cast to uint32)
    TimeMs      lastPress;      // synchronized time of last button press on sender
    TimeMs      currentTimeMs;  // sender's synchronized clock (millis() + EMA offset)
    ChangeIndex changeIndex;    // monotonic pattern-change counter
    Rnd         randomizer;     // pattern seed (low byte of lastPress)
    char        pattern[33];    // null-terminated pattern name, max 32 chars
};
static_assert(sizeof(HeartbeatPacket) <= 250, "HeartbeatPacket exceeds ESP-NOW 250-byte limit");

// ── Mesh class ───────────────────────────────────────────────────────────────

// Connectionless mesh over ESP-NOW broadcast. Each node broadcasts heartbeats
// to FF:FF:FF:FF:FF:FF every kHeartbeatIntervalMs. Join/leave events are
// synthesised from the heartbeat stream via timeout tracking.
class Mesh {
public:
    // Re-exported so Mesh::TimeMs and Mesh::NodeId remain valid access paths.
    using TimeMs      = Scarfnet::TimeMs;
    using NodeId      = Scarfnet::NodeId;
    using ChangeIndex = Scarfnet::ChangeIndex;

    using ReceivedCb  = std::function<void(const HeartbeatPacket&)>;
    using NodeCb      = std::function<void(NodeId)>;

    // Initialise WiFi in STA mode, start ESP-NOW, derive node ID from MAC.
    // Must be called before update() or broadcast().
    void begin();

    // Call every loop iteration. Drains the rx queue (filled by the WiFi task)
    // and fires node-leave callbacks for peers that have timed out.
    void update();

    // Node ID derived from the last 4 bytes of this device's MAC address.
    NodeId nodeId()    const { return _nodeId; }

    // Synchronized millisecond clock: millis() + EMA-smoothed peer-offset.
    // Converges across the fleet as heartbeats exchange currentTimeMs values.
    TimeMs timeMs()    const;

    // Number of peer nodes currently tracked (excludes self).
    size_t nodeCount() const { return _peers.size(); }

    // Broadcast a heartbeat packet to all ESP-NOW peers on kEspNowChannel.
    void broadcast(const HeartbeatPacket& pkt);

    // Register callbacks. Each fires on the loop() task (not the WiFi task).
    void onReceived(ReceivedCb cb)   { _receivedCb = std::move(cb); }
    void onNodeJoined(NodeCb cb)     { _joinedCb   = std::move(cb); }
    void onNodeLeft(NodeCb cb)       { _leftCb     = std::move(cb); }

private:
    // ESP-NOW recv callback — runs on the WiFi task; enqueues to ring buffer.
    static void espNowRecvCb(const uint8_t* mac, const uint8_t* data, int len);
    // Processes one received packet on the loop task.
    void handleReceived(const uint8_t* mac, const uint8_t* data, int len);
    void checkNodeTimeouts();
    // Update EMA clock offset from a peer's reported time.
    void updateClock(TimeMs pktCurrentTimeMs);

    static NodeId macToNodeId(const uint8_t* mac);

    NodeId _nodeId       = 0;
    float  _clockOffset  = 0.0f;  // timeMs() = millis() + (int32_t)_clockOffset
    int    _clockSamples = 0;     // hard-set during warmup, EMA after

    struct PeerState { uint32_t lastSeenMs; };
    std::unordered_map<NodeId, PeerState> _peers;

    ReceivedCb _receivedCb;
    NodeCb     _joinedCb;
    NodeCb     _leftCb;

    // Ring buffer: espNowRecvCb (WiFi task) writes; update() (loop task) reads.
    static constexpr int kRxQueueSize = 8;
    struct RxEntry {
        uint8_t mac[6];
        uint8_t data[250];
        int     len;
    };
    portMUX_TYPE _rxMux;
    RxEntry      _rxQueue[kRxQueueSize];
    int          _rxHead = 0;  // next write index (WiFi task)
    int          _rxTail = 0;  // next read index  (loop task)
};

} // namespace Scarfnet

#endif // SCARFNET_EMBEDDED
