#pragma once

// When a normal-mode scarf may look for an OTA server. Hardware-free so the
// backoff is natively testable.
//
// Each automatic restart into receiver mode counts as an attempt until an
// update succeeds. After failed attempts the scarf waits, measured from boot,
// before trying again, a little longer each time up to a cap — so a download that
// always fails can't keep the strip frozen in receiver mode, while scarves
// that merely lost their turn on a busy server still get through.

#include "config.h"
#include <stdint.h>

namespace Scarfnet {

inline uint32_t autoOtaHoldoffMs(uint32_t failedAttempts)
{
    if (failedAttempts == 0) return 0;
    const uint32_t stepsToCap = (kOtaAutoBackoffMaxMs - kOtaAutoBackoffBaseMs) / kOtaAutoBackoffStepMs;
    if (failedAttempts - 1 >= stepsToCap) return kOtaAutoBackoffMaxMs;
    return kOtaAutoBackoffBaseMs + kOtaAutoBackoffStepMs * (failedAttempts - 1);
}

inline bool autoOtaMayScan(bool newerPeerSeen, uint32_t failedAttempts, uint32_t msSinceBoot)
{
    return newerPeerSeen && msSinceBoot >= autoOtaHoldoffMs(failedAttempts);
}

} // namespace Scarfnet
