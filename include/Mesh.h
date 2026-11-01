#pragma once

#include "defines.h"

#if SCARFNET_EMBEDDED

#include <esp_now.h>
#include <freertos/portmacro.h>
#include <functional>
#include <cstdint>

#include "config.h"
#include "mesh_types.h"
#include "clock_sync.h"
#include "heartbeat_packet.h"
#include "heartbeat_framer.h"
#include "node_tracker.h"
#include "rx_queue.h"
#include "log.h"

namespace Scarfnet {

// ── Mesh class ───────────────────────────────────────────────────────────────

// Connectionless mesh over ESP-NOW broadcast. Join/leave events are synthesised
// from the heartbeat stream by timeout. Peer tracking, clock sync and framing
// live in the hardware-free NodeTracker, ClockSync and HeartbeatFramer.
class Mesh {
public:
    using TimeMs      = Scarfnet::TimeMs;
    using NodeId      = Scarfnet::NodeId;

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

    // espNowRecvCb (WiFi task) pushes; update() (loop task) pops.
    struct PortMuxLock {
        portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        void lock()   { portENTER_CRITICAL(&mux); }
        void unlock() { portEXIT_CRITICAL(&mux); }
    };
    struct RxEntry {
        uint8_t mac[6];
        uint8_t data[HeartbeatFramer::kMaxWireLen];
        int     len;
    };
    static constexpr size_t kRxQueueSize = 16;
    RxQueue<RxEntry, kRxQueueSize, PortMuxLock> _rxQueue;
};

} // namespace Scarfnet

#endif // SCARFNET_EMBEDDED
