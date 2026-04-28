#pragma once

#include "defines.h"
#include "mesh_types.h"  // NodeId, TimeMs
#include "tap_tempo.h"   // BeatInfo

#include <stdint.h>
#include <string>

namespace Scarfnet
{

// Recent heartbeat arrival, used by the debug pattern for per-node flash effects.
struct NodeFlash {
    NodeId id;    // sender node ID
    TimeMs when;  // mesh time the heartbeat was received
};
constexpr int kMaxNodeFlashes = 16;

// All context a pattern needs to render one frame.
struct PatternContext {
    TimeMs        timeMs;    // synchronized mesh time — use for all animation timing
    CRGBPalette16 palette;   // current blended palette
    Rnd           rnd;       // fleet-synchronized seed (same on all scarves)
    Rnd           localRnd;  // per-device seed (unique per scarf, never transmitted)
    BeatInfo      beat;      // tap-tempo beat state

    // Mesh identity — populated by Scarf, used by the debug pattern.
    NodeId    nodeId;                          // this scarf's own node ID
    NodeId    lastPressId;                     // node that last changed the pattern
    NodeFlash recentFlashes[kMaxNodeFlashes];  // recent heartbeat arrivals (ring buffer)
    int       flashCount;                      // valid entries in recentFlashes
};

typedef std::function<void(Leds&, const PatternContext&)> PatternFcn;
typedef std::pair<std::string, PatternFcn> NamedPattern;
typedef std::vector<NamedPattern> PatternList;

// ── Shared pattern utilities ─────────────────────────────────────────────────

// Maps a Rnd seed deterministically into [low, high). Use in getPatternList
// lambdas to convert the seed into concrete parameter values for a pattern.
inline uint8_t rndRange(Rnd rnd, uint8_t low, uint8_t high) {
    return rnd % (high - low) + low;
}

// Maps a Rnd seed deterministically into [low, high). Use in getPatternList
// lambdas to convert the seed into concrete parameter values for a pattern.
inline uint16_t rndRange16(Rnd rnd, uint16_t low, uint16_t high) {
    return rnd % (high - low) + low;
}

// Returns the elapsed fraction of `period` at time `time` as a fract8 [0, 255].
// Uses unsigned arithmetic throughout — safe against signed-modulo glitches when
// timeMs wraps past INT32_MAX on long-running devices or the simulator.
inline fract8 timeFrac8(uint32_t time, uint32_t period) {
    return (uint8_t)((uint64_t)(time % period) * 255u / period);
}

// Returns brightness [0..255] for this device's assigned sub-slot within the
// last-2-beats wild zone of any 16, 32, or 64-beat phrase, or 0 otherwise.
//
// Those 2 beats are divided into 8 equal sub-slots (each ¼ beat long).
// Every device picks the one slot matching `(uint8_t)localRnd % 8`, so the
// fleet produces a staggered cascade rather than a simultaneous whitewash.
// Brightness decays 255→0 over the slot duration for a punchy attack.
inline uint8_t wildZoneBright(const BeatInfo& beat, Rnd localRnd) {
    if (!beat.isActive() || beat.intervalMs == 0) return 0;

    const uint8_t  mySlot   = (uint8_t)localRnd % 8;
    const uint32_t subDurMs = (uint32_t)beat.intervalMs / 4;  // 2 beats / 8 slots = ¼ beat each
    if (subDurMs == 0) return 0;

    uint8_t best = 0;
    const uint16_t phraseLens[3] = {16, 32, 64};
    for (int p = 0; p < 3; ++p) {
        uint16_t phraseLen    = phraseLens[p];
        uint16_t beatInPhrase = beat.beatNumber % phraseLen;
        if (beatInPhrase < phraseLen - 2) continue;

        uint32_t wildPosMs = (uint32_t)(beatInPhrase - (phraseLen - 2)) * beat.intervalMs
                           + beat.phaseMs;                    // [0, 2*intervalMs)
        uint8_t  slot      = (uint8_t)(wildPosMs * 8u / (2u * beat.intervalMs));
        if (slot != mySlot) continue;

        uint32_t posInSub  = wildPosMs % subDurMs;
        uint8_t  bri       = (uint8_t)(255u - posInSub * 255u / subDurMs);
        if (bri > best) best = bri;
    }
    return best;
}

// ── Pattern render functions (implemented in src/patterns/*.cpp) ─────────────

void pride(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, Rnd rnd, Rnd localRnd);
void confetti(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, uint8_t fade, uint16_t popChancePct);
void firework(Leds& leds, const PatternContext& ctx);
void colorwaves(Leds& leds, const PatternContext& ctx);
void cylon(Leds& leds, const PatternContext& ctx);
void fractal(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, uint8_t spatialScale, Rnd rnd);
void breathe(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, int32_t periodMs, uint8_t hueSpeed, Rnd rnd);
void sparkle(Leds& leds, const PatternContext& ctx);
void dance(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, uint8_t hueSpeed, uint8_t macroPeriod, Rnd localRnd);
void generative(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, Rnd rnd, Rnd localRnd);
void beat(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, Rnd rnd, Rnd localRnd);
void digital(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const PatternContext& ctx);
void debug(Leds& leds, const PatternContext& ctx);


// ── Pattern registry ─────────────────────────────────────────────────────────

PatternList getPatternList();

}
