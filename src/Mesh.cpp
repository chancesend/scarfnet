#include "Mesh.h"

#ifdef SCARFNET_EMBEDDED

#include "config.h"
#include "swarm_ema.h"
#include "log.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <Arduino.h>
#include <cstring>
#include <cmath>

namespace Scarfnet {

// Singleton pointer used by the static ESP-NOW callback.
static Mesh* gMeshInstance = nullptr;

static constexpr uint8_t kBroadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ─── Helpers ────────────────────────────────────────────────────────────────

/*static*/ Mesh::NodeId Mesh::macToNodeId(const uint8_t* mac) {
    return ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) |
           ((uint32_t)mac[4] <<  8) |  (uint32_t)mac[5];
}

Mesh::TimeMs Mesh::timeMs() const {
    return (Mesh::TimeMs)((int32_t)millis() + (int32_t)_clockOffset);
}

// ─── Lifecycle ──────────────────────────────────────────────────────────────

void Mesh::begin() {
    portMUX_INITIALIZE(&_rxMux);
    gMeshInstance = this;

    uint8_t mac[6];
    WiFi.macAddress(mac);
    _nodeId = macToNodeId(mac);
    Scarfnet::log("[MESH] node ID %u  MAC %02X:%02X:%02X:%02X:%02X:%02X",
                  _nodeId, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // STA mode without connecting — required for ESP-NOW to function.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);

    // Fix the channel so all scarves can hear each other.
    esp_wifi_set_channel(kEspNowChannel, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Scarfnet::log("[MESH] esp_now_init() FAILED");
        while (true) delay(1000);
    }

    esp_now_register_recv_cb(Mesh::espNowRecvCb);

    // Register the broadcast peer so esp_now_send() accepts FF:FF:FF:FF:FF:FF.
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, kBroadcastMac, 6);
    peer.channel = kEspNowChannel;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Scarfnet::log("[MESH] failed to add broadcast peer");
    }

    Scarfnet::log("[MESH] ESP-NOW ready on channel %u", kEspNowChannel);
}

// ─── Loop ───────────────────────────────────────────────────────────────────

void Mesh::update() {
    // Drain the rx queue (filled by espNowRecvCb on the WiFi task).
    while (true) {
        RxEntry entry;
        bool hasEntry = false;

        portENTER_CRITICAL(&_rxMux);
        if (_rxTail != _rxHead) {
            entry   = _rxQueue[_rxTail];
            _rxTail = (_rxTail + 1) % kRxQueueSize;
            hasEntry = true;
        }
        portEXIT_CRITICAL(&_rxMux);

        if (!hasEntry) break;
        handleReceived(entry.mac, entry.data, entry.len);
    }

    checkNodeTimeouts();
}

// ─── ESP-NOW callback (WiFi task) ───────────────────────────────────────────

/*static*/ void Mesh::espNowRecvCb(const uint8_t* mac, const uint8_t* data, int len) {
    Mesh* self = gMeshInstance;
    if (!self) return;

    portENTER_CRITICAL(&self->_rxMux);
    int next = (self->_rxHead + 1) % kRxQueueSize;
    if (next != self->_rxTail) {
        RxEntry& slot = self->_rxQueue[self->_rxHead];
        memcpy(slot.mac,  mac,  6);
        int copyLen = len < 250 ? len : 250;
        memcpy(slot.data, data, copyLen);
        slot.len      = copyLen;
        self->_rxHead = next;
    } else {
        // Queue full; this packet is dropped. At 5-second heartbeat intervals
        // with 16 nodes, the queue should never fill.
        Scarfnet::log("[MESH] rx queue full — packet dropped");
    }
    portEXIT_CRITICAL(&self->_rxMux);
}

// ─── Receive (loop task) ────────────────────────────────────────────────────

void Mesh::handleReceived(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < (int)sizeof(HeartbeatPacket)) {
        Scarfnet::log("[MESH][RCV] short packet (%d bytes) — ignoring", len);
        return;
    }

    HeartbeatPacket pkt;
    memcpy(&pkt, data, sizeof(HeartbeatPacket));
    pkt.pattern[sizeof(pkt.pattern) - 1] = '\0';  // guard against unterminated data

    uint32_t nodeId = macToNodeId(mac);
    uint32_t now    = millis();

    bool isNew = (_peers.find(nodeId) == _peers.end());
    _peers[nodeId].lastSeenMs = now;

    updateClock(pkt.currentTimeMs);

    Scarfnet::log("[MESH][RCV node %u] pattern=%s ci=%u peerTime=%u myTime=%u",
                  nodeId, pkt.pattern, pkt.changeIndex, pkt.currentTimeMs, timeMs());

    if (isNew) {
        Scarfnet::log("[MESH] node %u joined (peers: %u)", nodeId, (unsigned)_peers.size());
        if (_joinedCb) _joinedCb(nodeId);
    }

    if (_receivedCb) _receivedCb(pkt);
}

void Mesh::checkNodeTimeouts() {
    uint32_t now = millis();
    for (auto it = _peers.begin(); it != _peers.end(); ) {
        if (now - it->second.lastSeenMs > kNodeTimeoutMs) {
            uint32_t nodeId = it->first;
            Scarfnet::log("[MESH] node %u timed out (peers: %u)", nodeId, (unsigned)_peers.size() - 1);
            it = _peers.erase(it);
            if (_leftCb) _leftCb(nodeId);
        } else {
            ++it;
        }
    }
}

// ─── Clock sync ─────────────────────────────────────────────────────────────

void Mesh::updateClock(TimeMs pktCurrentTimeMs) {
    // rawDelta = how far our millis() lags behind the peer's synced time.
    // Positive → peer is ahead; negative → peer is behind.
    int32_t rawDelta = (int32_t)pktCurrentTimeMs - (int32_t)millis();

    if (_clockSamples < kClockWarmupSamples) {
        // Hard-set during warmup so a freshly-booted node converges immediately,
        // even when the offset is orders of magnitude larger than the EMA window.
        _clockOffset = (float)rawDelta;
        _clockSamples++;
        Scarfnet::log("[SYNC] warmup %d/%d offset=%dms",
                      _clockSamples, kClockWarmupSamples, rawDelta);
        return;
    }

    // After warmup, only accept samples within ±kSwarmMaxClockDeviationMs of the
    // current estimate to guard against corrupted packets or extreme clock jumps.
    float deviation = fabsf((float)rawDelta - _clockOffset);
    if (deviation > (float)kSwarmMaxClockDeviationMs) {
        Scarfnet::log("[SYNC] deviation=%.0fms exceeds ±%dms — discarded",
                      deviation, kSwarmMaxClockDeviationMs);
        return;
    }

    _clockOffset = kSwarmEmaAlpha * (float)rawDelta + (1.0f - kSwarmEmaAlpha) * _clockOffset;
    Scarfnet::log("[SYNC] offset=%.0fms raw=%dms", _clockOffset, rawDelta);
}

// ─── Send ───────────────────────────────────────────────────────────────────

void Mesh::broadcast(const HeartbeatPacket& pkt) {
    esp_err_t result = esp_now_send(kBroadcastMac,
                                    reinterpret_cast<const uint8_t*>(&pkt),
                                    sizeof(HeartbeatPacket));
    if (result != ESP_OK) {
        Scarfnet::log("[MESH][SND] esp_now_send error %d", (int)result);
    }
}

} // namespace Scarfnet

#endif // SCARFNET_EMBEDDED
