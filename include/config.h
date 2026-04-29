#pragma once

#include <stdint.h>
#include <cstddef>

// ── Scarf firmware version ───────────────────────────────────────────────────
// Single source of truth for the firmware version. Increment before each OTA
// flash. Stamped into heartbeat packets and used in the OTA SSID/info endpoint.
// Version 100 = 1.00, version 102 = 1.02, etc
const uint16_t kScarfVersion              = 103;

// ── ObservableButton ─────────────────────────────────────────────────────────
const uint32_t kButtonPollIntervalMs       = 50;
const uint32_t kLongPressMs               = 1000;
const uint32_t kExtraLongPressMs          = 7000;

// ── Scarf: LED animation ─────────────────────────────────────────────────────
const int kLedRefreshRateMs               = 15;
const int kPaletteBlendRateMs             = 40;

// ── Scarf: heartbeat & blink ─────────────────────────────────────────────────
const uint32_t kHeartbeatIntervalMs       = 5000;
// Random ±jitter applied to each heartbeat interval so nodes on identical
// schedules naturally drift apart and stop colliding.
const uint32_t kHeartbeatJitterMs         = 200;
const uint32_t kMemLogIntervalMs          = 60000;
const int kNodeBlinkPeriodMs              = 3000;
const uint32_t kSyncBlinkPeriodMs         = 5000;

// ── Scarf: burst sync after node join ────────────────────────────────────────
// When a new node is detected, send kBurstSyncCount extra heartbeats at
// kBurstSyncIntervalMs so the joining node converges pattern and clock quickly.
const uint32_t kBurstSyncIntervalMs       = 500;
const int      kBurstSyncCount            = 3;

// ── Tap-tempo ────────────────────────────────────────────────────────────────
// Set true  → long press toggles tap-tempo mode (short press feeds the beat).
// Set false → long press restores original "same pattern, new randomizer" behaviour.
const bool  kTapTempoOnLongPress          = true;
// Taps whose implied BPM falls outside this range are ignored.
const float kTapMinBpm                    = 30.0f;
const float kTapMaxBpm                    = 300.0f;

// ── ESP-NOW ───────────────────────────────────────────────────────────────────
// All scarves must use the same channel. Channel 6 avoids overlap with channels
// 1 and 11 (the other common non-overlapping 2.4 GHz channels).
const uint8_t  kEspNowChannel             = 6;

// ── ScarfNet identity ────────────────────────────────────────────────────────
// Logical network ID stamped on every heartbeat. Packets from a different ID
// are silently dropped. Eventually this will be settable per-device from
// Preferences (call Mesh::setScarfNetId() at startup after loading). For now
// the compile-time default is used by all scarves.
// 0x5343524E = "SCRN" in ASCII.
const uint32_t kDefaultScarfNetId         = 0x5343524E;

// ── Mesh: node tracking ───────────────────────────────────────────────────────
// A peer is considered gone if no heartbeat is received within this window.
// 3 × heartbeat interval gives tolerance for two missed beats.
const uint32_t kNodeTimeoutMs             = 15000;

// ── Clock sync EMA ────────────────────────────────────────────────────────────
// Hard-set the clock offset for the first N heartbeats received (warmup).
// After warmup, the EMA (kSwarmEmaAlpha from swarm_ema.h) takes over.
const int      kClockWarmupSamples        = 3;

// Samples whose deviation from the current estimate exceeds this bound are
// discarded. Guards against corrupted packets and extreme clock jumps after
// warmup has established a baseline.
const int32_t  kSwarmMaxClockDeviationMs  = 5000;

// ── OTA ──────────────────────────────────────────────────────────────────────
const int      kOtaHoldMs                 = 10000;
const int      kOtaBootBlinkHalfPeriodMs  = 200;
const uint32_t kOtaIdleBlinkHalfPeriodMs  = 500;
const uint32_t kOtaConnectLedHalfPeriodMs = 150;
const uint32_t kOtaScanIntervalMs         = 8000;
const uint32_t kOtaConnectTimeoutMs       = 15000;
const size_t   kOtaFlashChunkBytes        = 512;
