#pragma once

// defines.h sets SCARFNET_EMBEDDED for the embedded target; include it first
// so the guard below works regardless of include order in the caller.
#include "defines.h"

#ifdef SCARFNET_EMBEDDED

#include <esp_now.h>
#include <freertos/portmacro.h>
#include <functional>
#include <cstdint>

#include "mesh_types.h"       // NodeId, TimeMs
#include "sync.h"             // ChangeIndex
#include "clock_sync.h"       // ClockSync
#include "heartbeat_packet.h" // HeartbeatPacket
#include "heartbeat_framer.h" // HeartbeatFramer
#include "node_tracker.h"     // NodeTracker
#include "log.h"

namespace Scarfnet {

// ── Mesh class ───────────────────────────────────────────────────────────────

// Connectionless mesh over ESP-NOW broadcast. Each node broadcasts heartbeats
// to FF:FF:FF:FF:FF:FF every kHeartbeatIntervalMs. Join/leave events are
// synthesised from the heartbeat stream via timeout tracking.
//
// Transport concerns are delegated to standalone units:
//   NodeTracker     — peer map, join/leave synthesis  (include/node_tracker.h)
//   ClockSync       — EMA clock offset                (include/clock_sync.h)
//   HeartbeatFramer — packet encode/decode            (include/heartbeat_framer.h)
class Mesh {
public:
    // Re-exported so Mesh::TimeMs / Mesh::NodeId / Mesh::ChangeIndex remain
    // valid access paths for callers that already use those qualified names.
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
    size_t nodeCount() const { return _tracker.count(); }

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
    // Update EMA clock offset from a peer's reported time, with logging.
    void updateClock(TimeMs pktCurrentTimeMs);

    static NodeId macToNodeId(const uint8_t* mac);

    NodeId      _nodeId  = 0;
    ClockSync   _clock;    // EMA clock offset
    NodeTracker _tracker;  // peer map + join/leave synthesis

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
