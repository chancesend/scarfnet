#pragma once

// A scarf's replicated state and the decisions it makes about it: what to do
// with a peer's heartbeat, and what a local press or tap changes. Hardware-free
// so the whole receive path is natively testable, including many scarves on a
// lossy network. Scarf supplies the hardware (pattern engine, radio, LEDs) and
// logs from the returned outcome.

#include "config.h"
#include "heartbeat_packet.h"
#include "mesh_types.h"
#include "sync_state.h"
#include "tempo_sync.h"

#include <functional>

namespace Scarfnet {

class SyncController
{
public:
    // Switch to the named pattern with this randomizer; false if the name is
    // not in this firmware's pattern set.
    using ApplyPattern = std::function<bool(const char* pattern, uint16_t randomizer)>;

    struct Received {
        TempoSync::Result tempo;
        UpdateDecision    decision;
        bool              unknownPattern;  // newer state named a pattern we don't have
        bool              rebroadcast;     // we adopted something newer: pass it on
    };

    Received receive(const HeartbeatPacket& pkt, const char* currentPattern,
                     const ApplyPattern& applyPattern)
    {
        if (pkt.version > kScarfVersion)
            _newerVersionSeen = true;

        Received r {};
        r.tempo = _tempo.receive(pkt.tempoIndex, pkt.beatIntervalMs, pkt.currentTimeMs, pkt.beatPhaseMs);
        r.rebroadcast = r.tempo != TempoSync::Result::Ignored;

        r.decision = classifyUpdate(pkt, _sync, currentPattern);
        if (r.decision != UpdateDecision::Accept)
            return r;

        if (!applyPattern(pkt.pattern, pkt.globalRandomizer)) {
            // Don't adopt the index: every later heartbeat retries (and raises
            // the alarm again) until a pattern we know wins.
            r.unknownPattern = true;
            return r;
        }
        // Adopts the randomizer too, so our heartbeats relay the presser's
        // variation rather than our own stale one. See sync_state.h.
        adoptSyncState(_sync, pkt);
        r.rebroadcast = true;
        return r;
    }

    // A local press that changes the look: claim the next change index with
    // `randomizer`. The caller applies the pattern change.
    void localPress(TimeMs now, NodeId self, uint16_t randomizer)
    {
        _sync.lastPressMs = now;
        _sync.lastPressId = self;
        _sync.randomizer  = randomizer;
        _sync.changeIndex += 1;
    }

    void toggleTapMode()
    {
        if (_tempo.tapMode) _tempo.exitTapMode();
        else                _tempo.enterTapMode();
    }

    void tap(TimeMs now) { _tempo.tap(now); }

    const SyncState& state()            const { return _sync; }
    const TempoSync& tempo()            const { return _tempo; }
    bool             newerVersionSeen() const { return _newerVersionSeen; }

private:
    SyncState _sync {};
    TempoSync _tempo;
    bool      _newerVersionSeen {false};
};

} // namespace Scarfnet
