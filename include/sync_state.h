#pragma once

// The replicated pattern-change state a scarf carries in every heartbeat, and
// the two places it crosses the wire: written into outgoing packets and adopted
// from accepted ones. Kept hardware-free so the relay round-trip is unit-tested
// natively — every field adopted here must be one we relay, or a scarf that
// hears the update second-hand ends up with a different state at the same
// changeIndex, which no later heartbeat can correct.

#include "heartbeat_packet.h"
#include "sync.h"

namespace Scarfnet {

struct SyncState {
    ChangeIndex changeIndex;
    TimeMs      lastPressMs;   // synchronized time of the press that set this state
    NodeId      lastPressId;   // node that pressed
    uint16_t    randomizer;    // presser's randomizer (Rnd); selects palette/variation
};

inline void writeSyncState(HeartbeatPacket& pkt, const SyncState& state)
{
    pkt.changeIndex      = state.changeIndex;
    pkt.lastPressMs      = state.lastPressMs;
    pkt.lastPressId      = state.lastPressId;
    pkt.globalRandomizer = state.randomizer;
}

// Call only after shouldAcceptUpdate() passed and the pattern was applied.
inline void adoptSyncState(SyncState& state, const HeartbeatPacket& pkt)
{
    state.changeIndex = rolloverGuard(pkt.changeIndex);
    state.lastPressMs = rolloverGuard(pkt.lastPressMs);
    state.lastPressId = pkt.lastPressId;
    state.randomizer  = pkt.globalRandomizer;
}

} // namespace Scarfnet
