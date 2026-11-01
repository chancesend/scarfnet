#pragma once

#include <stdint.h>
#include <cstddef>

// ── Scarf firmware version ───────────────────────────────────────────────────
// Single source of truth for the firmware version. Increment before each OTA
// flash. Stamped into heartbeat packets and the OTA SSID. 115 = v1.15.
const uint16_t kScarfVersion              = 115;

// ── Logging ──────────────────────────────────────────────────────────────────
// Per-heartbeat send/receive/clock lines. Serial output blocks the loop once
// the UART FIFO fills, so leave this off in the field.
const bool     kVerboseLogging            = false;

// ── ObservableButton ─────────────────────────────────────────────────────────
const uint32_t kButtonPollIntervalMs      = 50;
const uint32_t kButtonDebounceMs          = 10;
const uint32_t kLongPressMs               = 1000;
const uint32_t kExtraLongPressMs          = 7000;

// ── Scarf: LED output ────────────────────────────────────────────────────────
const uint32_t kLedRefreshRateMs          = 15;
const uint32_t kPaletteBlendRateMs        = 30;
const uint32_t kPaletteTransitionMs       = 2000;
const uint32_t kPatternTransitionMs       = 500;
const uint32_t kLedStripPowerLimitMw      = 500;
// The built-in LED is re-shown after FastLED's power limiting, so it keeps
// this brightness however dim the strip is scaled.
const uint8_t  kBuiltinLedBrightness      = 128;
// Pause before restarting after the LED type changes, so the log line flushes.
const uint32_t kLedTypeRestartDelayMs     = 1000;

// ── Scarf: heartbeat ─────────────────────────────────────────────────────────
const uint32_t kHeartbeatIntervalMs       = 5000;
// Random ±jitter on each heartbeat interval, so nodes on identical schedules
// drift apart and stop colliding.
const uint32_t kHeartbeatJitterMs         = 200;
const uint32_t kMemLogIntervalMs          = 60000;
const uint32_t kSyncBlinkPeriodMs         = 5000;

// ── Scarf: burst sync after node join ────────────────────────────────────────
// Extra heartbeats sent when a node joins, so it converges pattern and clock
// quickly.
const uint32_t kBurstSyncIntervalMs       = 500;
const int      kBurstSyncCount            = 3;
// Every peer hears a join at the same moment; without jitter on the first
// burst heartbeat their bursts collide on air.
const uint32_t kBurstSyncJitterMs         = 250;

// ── Scarf: epidemic re-broadcast on accept ───────────────────────────────────
// A scarf that accepts a new state re-broadcasts it at once, so the update
// floods the network instead of trickling at the heartbeat cadence. Duplicates
// fail the sync check on arrival, so each node re-emits at most once per
// update. The random delay keeps nodes from transmitting simultaneously and
// overflowing the rx queue.
const uint32_t kRebroadcastJitterMs       = 250;

// ── Scarf: unknown-pattern alarm ─────────────────────────────────────────────
// Built-in LED flashes red when a peer names a pattern this firmware doesn't
// have. Retriggers on every such packet, so a stuck scarf keeps flashing.
const uint32_t kUnknownPatternFlashCount        = 4;
const uint32_t kUnknownPatternFlashHalfPeriodMs = 80;

// ── Tap-tempo ────────────────────────────────────────────────────────────────
// true  → long press toggles tap-tempo mode (short press feeds the beat).
// false → long press keeps the pattern with a new randomizer.
const bool     kTapTempoOnLongPress       = true;
const float    kTapMinBpm                 = 30.0f;
const float    kTapMaxBpm                 = 300.0f;
// A longer gap between taps starts a new tap sequence.
const uint32_t kTapMaxGapMs               = 3000;

// ── ESP-NOW ──────────────────────────────────────────────────────────────────
// All scarves must share a channel. 6 avoids overlap with 1 and 11.
const uint8_t  kEspNowChannel             = 6;
// esp_now_init() failure is unrecoverable in place: wait, then restart.
const uint32_t kEspNowInitFailRestartMs   = 1000;

// ── ScarfNet identity ────────────────────────────────────────────────────────
// Logical network ID stamped on every heartbeat; packets with another ID are
// dropped. "SCRN" in ASCII.
const uint32_t kDefaultScarfNetId         = 0x5343524E;

// ── Mesh: node tracking ──────────────────────────────────────────────────────
// Tolerates two missed heartbeats before a peer counts as gone.
const uint32_t kNodeTimeoutMs             = 3 * kHeartbeatIntervalMs;

// ── Clock sync ───────────────────────────────────────────────────────────────
// The first samples hard-set the offset: a fresh boot is hours away from the
// network clock, too far to track.
const int      kClockWarmupSamples        = 3;

// After warmup, a sample further than this from our estimate is from another
// clock domain: jump to it if ahead, ignore it if behind (see clock_sync.h).
const int32_t  kClockMaxDeviationMs       = 5000;

// Weight of each new in-range sample in the offset estimate.
const float    kClockEmaAlpha             = 0.4f;

// In-range corrections are slewed: the synced clock runs at most this fraction
// faster or slower than real time until it reaches the target, so it never
// runs backward.
const float    kClockSlewRate             = 0.5f;

// ── OTA ──────────────────────────────────────────────────────────────────────
const uint32_t kOtaHoldMs                 = 5000;
const uint32_t kOtaBootBlinkHalfPeriodMs  = 200;
const uint32_t kOtaIdleBlinkHalfPeriodMs  = 500;
const uint32_t kOtaConnectLedHalfPeriodMs = 150;
const uint32_t kOtaScanIntervalMs         = 3000;
const uint32_t kOtaConnectTimeoutMs       = 15000;
const uint32_t kOtaConnectPollMs          = 10;
const size_t   kOtaFlashChunkBytes        = 4096;  // one flash sector
// Server gives up on one receiver's download after this long, so a stalled
// client can't block every other scarf waiting to update.
const uint32_t kOtaServeTimeoutMs         = 60000;
// The server streams to one receiver at a time, so a receiver may wait for
// another's download; waiting beats timing out and losing its turn.
const uint32_t kOtaHttpTimeoutMs          = 30000;
const uint32_t kOtaStreamReadTimeoutMs    = 10000;

// Receiver LED feedback.
const int      kOtaServerFoundStrobeCount        = 3;
const uint32_t kOtaServerFoundStrobeHalfPeriodMs = 80;
const int      kOtaInfoVerifiedStrobeCount        = 4;
const uint32_t kOtaInfoVerifiedStrobeHalfPeriodMs = 60;
// Download progress pulse speeds up from the first to the second half-period.
const uint32_t kOtaProgressPulseStartHalfPeriodMs = 1000;
const uint32_t kOtaProgressPulseEndHalfPeriodMs   = 50;
const uint32_t kOtaSuccessDisplayMs       = 2000;
// Pause before an OTA restart, so the log line flushes.
const uint32_t kOtaRestartFlushMs         = 100;

// ── OTA: automatic update ────────────────────────────────────────────────────
// Once a peer on newer firmware has been seen, scan the ESP-NOW channel in the
// background for an OTA server AP (it shares the channel, so the radio never
// leaves the mesh). A newer server restarts this scarf into receiver mode,
// which restarts back to normal if no update completes in time.
const uint32_t kOtaAutoScanIntervalMs     = 10000;
const uint32_t kOtaAutoScanDwellMs        = 150;
const uint32_t kOtaAutoTimeoutMs          = 120000;
