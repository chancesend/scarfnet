#pragma once

#include "defines.h"

#if SCARFNET_EMBEDDED

#include <esp_now.h>
#include <freertos/portmacro.h>
#include <functional>
#include <cstdint>

#include "config.h"
#include "mesh_types.h"
#include "sync.h"
#include "clock_sync.h"
#include "heartbeat_packet.h"
#include "heartbeat_framer.h"
#include "node_tracker.h"
#include "log.h"

namespace Scarfnet {

// ── Mesh class ───────────────────────────────────────────────────────────────

// Connectionless mesh over ESP-NOW broadcast. Join/leave events are synthesised
// from the heartbeat stream by timeout. Peer tracking, clock sync and framing
// live in the hardware-free NodeTracker, ClockSync and HeartbeatFramer.
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

    // Network clock, converged across the fleet from heartbeat timestamps.
    TimeMs timeMs()    const;

    // Number of peer nodes currently tracked (excludes self).
    size_t nodeCount() const { return _tracker.count(); }

    void broadcast(const HeartbeatPacket& pkt);

    // Register callbacks. Each fires on the loop() task (not the WiFi task).
    void onReceived(ReceivedCb cb)   { _receivedCb = std::move(cb); }
    void onNodeJoined(NodeCb cb)     { _joinedCb   = std::move(cb); }
    void onNodeLeft(NodeCb cb)       { _leftCb     = std::move(cb); }

    // Join a ScarfNet other than kDefaultScarfNetId.
    void setScarfNetId(ScarfNetId id) { _scarfNetId = id; }

private:
    // ESP-NOW recv callback — runs on the WiFi task; enqueues to ring buffer.
    static void espNowRecvCb(const uint8_t* mac, const uint8_t* data, int len);
    // Processes one received packet on the loop task.
    void handleReceived(const uint8_t* mac, const uint8_t* data, int len);
    // Feed a peer's timestamp to the clock estimate, with logging.
    void updateClock(TimeMs pktCurrentTimeMs);

    static NodeId macToNodeId(const uint8_t* mac);

    NodeId      _nodeId     = 0;
    ScarfNetId  _scarfNetId = kDefaultScarfNetId;
    ClockSync   _clock;
    NodeTracker _tracker;

    ReceivedCb _receivedCb;
    NodeCb     _joinedCb;
    NodeCb     _leftCb;

    // Ring buffer: espNowRecvCb (WiFi task) writes; update() (loop task) reads.
    static constexpr int kRxQueueSize = 16;
    struct RxEntry {
        uint8_t mac[6];
        uint8_t data[HeartbeatFramer::kMaxWireLen];
        int     len;
    };
    portMUX_TYPE _rxMux;
    RxEntry      _rxQueue[kRxQueueSize];
    int          _rxHead = 0;  // next write index (WiFi task)
    int          _rxTail = 0;  // next read index  (loop task)
    uint32_t     _rxDropped = 0;  // packets dropped on a full queue; logged by update()
};

} // namespace Scarfnet

#endif // SCARFNET_EMBEDDED
