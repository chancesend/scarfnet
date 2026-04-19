#pragma once

// Peer-presence tracking and join/leave event synthesis.
// No hardware deps — can be unit-tested natively.
//
// Each heartbeat arrives with a NodeId. NodeTracker remembers the last-seen
// time for each peer. Callers:
//   1. Call saw() on every received packet → get back whether this is a new node.
//   2. Call checkTimeouts() every loop iteration → callbacks fire for stale peers.

#include "mesh_types.h"  // NodeId

#include <functional>
#include <unordered_map>
#include <cstdint>

namespace Scarfnet {

struct NodeTracker {
    using LeaveCb = std::function<void(NodeId)>;

    // Record a heartbeat from `nodeId` at local time `nowMs`.
    // Returns true if this is the first heartbeat from this node (join event).
    bool saw(NodeId nodeId, TimeMs nowMs) {
        bool isNew = (_peers.find(nodeId) == _peers.end());
        _peers[nodeId] = nowMs;
        return isNew;
    }

    // Erase peers whose last-seen time is more than `timeoutMs` ago.
    // Calls `leaveCb` for each removed peer. Returns the number removed.
    int checkTimeouts(TimeMs nowMs, TimeMs timeoutMs, const LeaveCb& leaveCb) {
        int removed = 0;
        for (auto it = _peers.begin(); it != _peers.end(); ) {
            if (nowMs - it->second > timeoutMs) {
                if (leaveCb) leaveCb(it->first);
                it = _peers.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    // Number of currently-tracked peers (excludes self).
    size_t count() const { return _peers.size(); }

private:
    std::unordered_map<NodeId, TimeMs> _peers;  // nodeId → lastSeenMs
};

} // namespace Scarfnet
