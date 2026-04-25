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
inline fract8 timeFrac8(int time, int period) {
    return (time % period) * 255 / period;
}

// ── Pattern render functions (implemented in src/patterns/*.cpp) ─────────────

void pride(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat);
void confetti(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, uint8_t fade, uint16_t popChancePct);
void firework(Leds& leds, int32_t timeMs, int periodMs, const CRGBPalette16& palette);
void colorwaves(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat);
void cylon(Leds& leds, int32_t timeMs, CRGB color, int width, int periodMs, fract8 blurAmount);
void fractal(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, uint8_t spatialScale, Rnd rnd);
void breathe(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, int32_t periodMs, uint8_t hueSpeed, Rnd rnd);
void sparkle(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, uint8_t sparkleRate);
void dance(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, uint8_t hueSpeed, uint8_t macroPeriod);
void generative(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, Rnd rnd, Rnd localRnd);
void fillNoise(Leds& leds, int32_t timeMs);
void digital(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const PatternContext& ctx);
void debug(Leds& leds, const PatternContext& ctx);


// ── Pattern registry ─────────────────────────────────────────────────────────

PatternList getPatternList();

}
