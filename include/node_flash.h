#pragma once

// Recent heartbeat arrivals, one entry per sender, for the debug pattern's
// per-node flashes. Hardware-free; shared by the firmware and the simulator.

#include "mesh_types.h"

namespace Scarfnet {

struct NodeFlash {
    NodeId id;    // sender node ID
    TimeMs when;  // mesh time the heartbeat was received
};
constexpr int kMaxNodeFlashes = 16;

struct NodeFlashTable {
    NodeFlash entries[kMaxNodeFlashes] {};
    int       count {0};

    // Refresh the sender's entry, or add one; when full, replace the oldest.
    void record(NodeId id, TimeMs now)
    {
        for (int i = 0; i < count; i++) {
            if (entries[i].id == id) { entries[i].when = now; return; }
        }
        if (count < kMaxNodeFlashes) {
            entries[count++] = {id, now};
            return;
        }
        int oldest = 0;
        for (int i = 1; i < kMaxNodeFlashes; i++)
            if (timeDiff(entries[i].when, entries[oldest].when) < 0) oldest = i;
        entries[oldest] = {id, now};
    }
};

} // namespace Scarfnet
