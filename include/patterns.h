#pragma once

#include "defines.h"

#include <stdint.h>
#include <string>

namespace Scarfnet
{

typedef std::function<void(Leds&, int32_t, CRGBPalette16, Rnd)> PatternFcn;
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

void pride(Leds& leds, int32_t timeMs, const CRGBPalette16& palette);
void confetti(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, uint8_t fade, uint8_t popChancePct);
void firework(Leds& leds, int32_t timeMs, int periodMs, const CRGBPalette16& palette);
void colorwaves(Leds& leds, int32_t timeMs, const CRGBPalette16& palette);
void cylon(Leds& leds, int32_t timeMs, CRGB color, int width, int periodMs, fract8 blurAmount);
void fillNoise(Leds& leds, int32_t timeMs);

// ── Pattern registry ─────────────────────────────────────────────────────────

PatternList getPatternList();

}
