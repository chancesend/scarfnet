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
const uint32_t kHeartbeatIntervalMs       = 3000;
const uint32_t kMemLogIntervalMs          = 60000;
const int kNodeBlinkPeriodMs              = 3000;
const uint32_t kSyncBlinkPeriodMs         = 5000;

// ── OTA ──────────────────────────────────────────────────────────────────────
const int      kOtaHoldMs                 = 10000; // hold time for boot trigger or server-mode entry
const int      kOtaBootBlinkHalfPeriodMs  = 200;
const uint32_t kOtaIdleBlinkHalfPeriodMs  = 500;
const uint32_t kOtaConnectLedHalfPeriodMs = 150;
const uint32_t kOtaScanIntervalMs         = 8000;
const uint32_t kOtaConnectTimeoutMs       = 15000;
const size_t   kOtaFlashChunkBytes        = 512;
