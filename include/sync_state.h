#pragma once

// The replicated pattern-change state a scarf carries in every heartbeat, and
// the two places it crosses the wire: written into outgoing packets and adopted
// from accepted ones. Kept hardware-free so the relay round-trip is unit-tested
// natively — every field adopted here must be one we relay, or a scarf that
// hears the update second-hand ends up with a different state at the same
// changeIndex, which no later heartbeat can correct.

#include "heartbeat_packet.h"

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

// Total order over sync states: true if `incoming` should replace `local`.
//
// Higher changeIndex wins. At an equal changeIndex — two scarves pressed from
// the same index at nearly the same moment — the higher randomizer wins, then
// the higher presser id. Without this tie-break each scarf keeps whichever
// press it heard first and rejects the other as "not newer", splitting the
// network until the next press. The randomizer is fresh per press, so the
// winner is effectively a coin flip rather than always the same scarf; the
// presser id only decides a 1-in-65536 randomizer tie, and always does since
// two colliding presses come from different pressers. Identical states are
// not newer, so steady-state duplicates are still ignored.
// True if change index `a` is newer than `b`, allowing for 32-bit wraparound.
inline bool changeIndexNewer(ChangeIndex a, ChangeIndex b)
{
    return (int32_t)(a - b) > 0;
}

inline bool isNewerState(const SyncState& incoming, const SyncState& local)
{
    if (incoming.changeIndex != local.changeIndex)
        return changeIndexNewer(incoming.changeIndex, local.changeIndex);
    if (incoming.randomizer != local.randomizer)
        return incoming.randomizer > local.randomizer;
    return incoming.lastPressId > local.lastPressId;
}

// The sync state a packet carries.
inline SyncState syncStateOf(const HeartbeatPacket& pkt)
{
    return SyncState{pkt.changeIndex, pkt.lastPressMs, pkt.lastPressId, pkt.globalRandomizer};
}

enum class UpdateDecision : uint8_t {
    Accept,           // newer state: apply the pattern, then adoptSyncState()
    Ignore,           // not newer, same look: steady-state duplicate, silent
    RejectDiffering,  // not newer, different pattern or variation: this scarf
                      // or the sender is out of step — the "stuck scarf" signature
};

// What to do with an incoming heartbeat, given our state and current pattern.
inline UpdateDecision classifyUpdate(const HeartbeatPacket& pkt, const SyncState& local,
                                     const char* currentPattern)
{
    if (isNewerState(syncStateOf(pkt), local))
        return UpdateDecision::Accept;
    bool sameLook = strncmp(pkt.pattern, currentPattern, sizeof(pkt.pattern)) == 0 &&
                    pkt.globalRandomizer == local.randomizer;
    return sameLook ? UpdateDecision::Ignore : UpdateDecision::RejectDiffering;
}

// Call only after classifyUpdate() returned Accept and the pattern was applied.
inline void adoptSyncState(SyncState& state, const HeartbeatPacket& pkt)
{
    state.changeIndex = pkt.changeIndex;
    state.lastPressMs = pkt.lastPressMs;
    state.lastPressId = pkt.lastPressId;
    state.randomizer  = pkt.globalRandomizer;
}

} // namespace Scarfnet
