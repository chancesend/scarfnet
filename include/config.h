#pragma once

#include <stdint.h>
#include <cstddef>

// ── ObservableButton ─────────────────────────────────────────────────────────
const uint32_t kButtonPollIntervalMs       = 50;
const uint32_t kLongPressMs               = 1000;
const uint32_t kExtraLongPressMs          = 7000;

// ── Scarf: LED animation ─────────────────────────────────────────────────────
const int kLedRefreshRateMs               = 15;
const int kPaletteBlendRateMs             = 40;

// ── Scarf: heartbeat & blink ─────────────────────────────────────────────────
const uint32_t kHeartbeatIntervalMs       = 5000;
const uint32_t kMemLogIntervalMs          = 60000;
const int kNodeBlinkPeriodMs              = 3000;
const uint32_t kSyncBlinkPeriodMs         = 5000;

// ── SWARM arrival delta EMA ───────────────────────────────────────────────────
// Samples beyond this magnitude are discarded before entering the EMA.
const int32_t  kSwarmMaxArrivalDeltaMs    = 5000;

// Clock-stability gate: after a topology change, block EMA updates until this
// many consecutive time-sync adjustments have had |offset| below the threshold.
// Prevents the 1–5 s transient values (clock mostly settled but not fully) from
// entering the EMA even though they pass the ±5000 ms clamp.
const int      kSwarmSettleAdjustments       = 3;
const int32_t  kSwarmSettleOffsetThresholdUs = 200000;  // 200 ms

// ── Scarf: sync burst after connection change ─────────────────────────────────
// After a topology change, send kBurstSyncCount extra heartbeats at
// kBurstSyncIntervalMs so painlessMesh has more samples for time convergence.
const uint32_t kBurstSyncIntervalMs       = 500;
const int      kBurstSyncCount            = 3;


// ── OTA ──────────────────────────────────────────────────────────────────────
const int      kOtaHoldMs                 = 10000; // hold time for boot trigger or server-mode entry
const int      kOtaBootBlinkHalfPeriodMs  = 200;
const uint32_t kOtaIdleBlinkHalfPeriodMs  = 500;
const uint32_t kOtaConnectLedHalfPeriodMs = 150;
const uint32_t kOtaScanIntervalMs         = 8000;
const uint32_t kOtaConnectTimeoutMs       = 15000;
const size_t   kOtaFlashChunkBytes        = 512;
