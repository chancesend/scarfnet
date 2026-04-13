#pragma once

#include <stdint.h>
#include <stdio.h>

namespace Scarfnet
{

// Converts a raw painlessMesh node time (microseconds, uint32_t) to a
// millisecond timestamp with rollover tracking. Header-only so it can be
// included in native unit tests without the Arduino/painlessMesh stack.
//
// Packs the result as:
//   bits  0-21: lower 22 bits of nodeTimeMs
//   bits 22-31: rolloverCount
inline uint32_t computeNodeTimeMs(uint32_t rawNodeTime, int32_t& lastNodeTimeMs, int32_t& rolloverCount)
{
    const uint32_t kShift = 10; // divide microseconds by 1024 ≈ milliseconds
    const int32_t nodeTimeMs = (int32_t)(rawNodeTime >> kShift);
    const int32_t kRolloverThresholdMs = 1000 * 1000;
    if (nodeTimeMs - lastNodeTimeMs < -kRolloverThresholdMs)
    {
        rolloverCount++;
        printf("[MESH] getNodeTime() rollover!\n");
    }
    lastNodeTimeMs = nodeTimeMs;
    return ((uint32_t)nodeTimeMs & 0x003fffffu) | (((uint32_t)rolloverCount << (32u - kShift)) & 0xffc00000u);
}

} // namespace Scarfnet
