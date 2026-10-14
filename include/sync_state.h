#pragma once

// The replicated pattern-change state a scarf carries in every heartbeat, and
// the two places it crosses the wire: written into outgoing packets and adopted
// from accepted ones. Kept hardware-free so the relay round-trip is unit-tested
// natively — every field adopted here must be one we relay, or a scarf that
// hears the update second-hand ends up with a different state at the same
// changeIndex, which no later heartbeat can correct.

#include "heartbeat_packet.h"
#include "sync.h"

#include <string.h>

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

enum class UpdateDecision : uint8_t {
    Accept,           // newer state: apply the pattern, then adoptSyncState()
    Ignore,           // not newer, same pattern: steady-state duplicate, silent
    RejectDiffering,  // not newer, different pattern: this scarf or the sender
                      // is out of step — the field "stuck scarf" signature
};

// What to do with an incoming heartbeat, given our state and current pattern.
inline UpdateDecision classifyUpdate(const HeartbeatPacket& pkt, const SyncState& local,
                                     const char* currentPattern)
{
    if (shouldAcceptUpdate(pkt.changeIndex, local.changeIndex))
        return UpdateDecision::Accept;
    bool samePattern = strncmp(pkt.pattern, currentPattern, sizeof(pkt.pattern)) == 0;
    return samePattern ? UpdateDecision::Ignore : UpdateDecision::RejectDiffering;
}

// Call only after classifyUpdate() returned Accept and the pattern was applied.
inline void adoptSyncState(SyncState& state, const HeartbeatPacket& pkt)
{
    state.changeIndex = rolloverGuard(pkt.changeIndex);
    state.lastPressMs = rolloverGuard(pkt.lastPressMs);
    state.lastPressId = pkt.lastPressId;
    state.randomizer  = pkt.globalRandomizer;
}

} // namespace Scarfnet
