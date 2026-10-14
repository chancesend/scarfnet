#pragma once

// Assembles the outgoing heartbeat from a scarf's state. Hardware-free so the
// exact bytes a scarf relays are unit-tested natively.

#include "config.h"           // kScarfVersion
#include "heartbeat_packet.h"
#include "sync_state.h"
#include "tap_tempo.h"

#include <string>

namespace Scarfnet {

inline HeartbeatPacket buildHeartbeat(NodeId id, TimeMs now, const SyncState& sync,
                                      const std::string& pattern, const TapTempo& tapTempo)
{
    HeartbeatPacket pkt = {};
    pkt.id             = id;
    pkt.version        = kScarfVersion;
    pkt.currentTimeMs  = now;
    writeSyncState(pkt, sync);
    setPatternName(pkt, pattern);
    pkt.beatIntervalMs = tapTempo.beatIntervalMs();
    pkt.beatPhaseMs    = tapTempo.beatPhaseMs(now);
    return pkt;
}

} // namespace Scarfnet
