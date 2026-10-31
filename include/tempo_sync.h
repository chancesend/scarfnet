#pragma once

// Replicated tap-tempo state: which tempo the network is on, and whether this
// scarf is the one setting it. Hardware-free so it's natively testable.
//
// The tempo itself rides in the packet's beatIntervalMs/beatPhaseMs; this adds
// a 16-bit version (`tempoIndex`) so it's ordered like the pattern changeIndex:
//
//   - Every local tempo event bumps the index: entering tap mode, a tap that
//     leaves a tempo active, and exiting tap mode (which clears the tempo).
//   - Peers adopt the tempo only from a newer index, so stale or "no tempo"
//     heartbeats can't clear or overwrite it, and two tempos can't swap back
//     and forth.
//   - Only one scarf can be in tap mode: a scarf in tap mode that hears a newer
//     tempo state didn't create it (it knows its own latest index), so another
//     scarf has taken over — it leaves tap mode and follows the new tempo.
//   - At an equal index (two scarves entered or tapped at the same moment) the
//     higher beat interval wins, so every scarf picks the same tempo. Entering
//     at the same moment carries the same tempo, so both stay in tap mode until
//     one taps — which bumps the index and takes over from the other.
//   - Indices compare wrap-safely, so 65535 → 0 still counts as newer.

#include "mesh_types.h"  // TimeMs
#include "tap_tempo.h"
#include <stdint.h>

namespace Scarfnet {

// True if tempo index `a` is newer than `b`, allowing for 16-bit wraparound.
inline bool tempoIndexNewer(uint16_t a, uint16_t b)
{
    return (int16_t)(uint16_t)(a - b) > 0;
}

// Total order over tempo states: version first, then interval as tie-break.
inline bool isNewerTempo(uint16_t inIndex, uint16_t inIntervalMs,
                         uint16_t localIndex, uint16_t localIntervalMs)
{
    if (inIndex != localIndex) return tempoIndexNewer(inIndex, localIndex);
    return inIntervalMs > localIntervalMs;
}

struct TempoSync
{
    TapTempo tempo;
    uint16_t index   {0};
    bool     tapMode {false};  // this scarf is in tap-tempo input mode

    // Long-press into tap mode: become the tempo source, keeping the current tempo.
    void enterTapMode() { tapMode = true; ++index; }

    // Long-press out of tap mode: clear the tempo network-wide.
    void exitTapMode()  { tapMode = false; tempo.reset(); ++index; }

    // A tap in tap mode. Only a tap that leaves a tempo active is announced —
    // the first tap of a fresh sequence has no tempo yet and must not clear
    // the network's.
    void tap(TimeMs now)
    {
        tempo.tap(now);
        if (tempo.isActive()) ++index;
    }

    enum class Result { Ignored, Adopted, TakenOver };

    // Apply a peer heartbeat's tempo fields.
    Result receive(uint16_t pktIndex, uint16_t pktIntervalMs,
                   TimeMs pktCurrentTimeMs, uint16_t pktBeatPhaseMs)
    {
        if (!isNewerTempo(pktIndex, pktIntervalMs, index, tempo.beatIntervalMs()))
            return Result::Ignored;

        index = pktIndex;
        if (pktIntervalMs != 0) tempo.setFromPacket(pktIntervalMs, pktCurrentTimeMs, pktBeatPhaseMs);
        else                    tempo.reset();

        if (tapMode) {
            tapMode = false;
            return Result::TakenOver;
        }
        return Result::Adopted;
    }
};

} // namespace Scarfnet
