#include "patterns.h"

namespace Scarfnet {

// Three octaves of coherent noise (fractional Brownian motion).
// Coarser octaves dominate hue; finer octaves add shimmer and brightness detail.
//
// `spatialScale` controls the grain of the pattern — higher values produce
// tighter, more detailed structure; lower values produce broad slow waves.
// Derive it from rnd in the getPatternList lambda so scarves show variation.
//
// Time scales:
//   fine   — traverse one noise unit every ~2s   (fast shimmer)
//   mid    — traverse one noise unit every ~40s  (rolling structure)
//   coarse — traverse one noise unit every ~20min (slow macro drift)
//
// Fully stateless: all output is a pure function of (timeMs, i). No static
// variables, so there are no glitches when switching patterns.
void fractal(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
             const BeatInfo& beat, uint8_t spatialScale)
{
    const uint32_t t = (uint32_t)timeMs;

    for (int i = 0; i < kNumLeds; ++i) {
        const uint8_t x = (uint8_t)(i * spatialScale);

        const uint8_t fine   = inoise8(x,       t /    8);  // ~2s
        const uint8_t mid    = inoise8(x + 100, t /  150);  // ~40s
        const uint8_t coarse = inoise8(x + 200, t / 4800);  // ~20min

        // fBm blend: coarser octave drives most of the hue so the slow drift
        // is clearly visible; finer octaves contribute detail and shimmer.
        uint8_t hue        = (coarse >> 1) + (mid >> 2) + (fine >> 2);
        uint8_t brightness = lerp8by8(160, 255, qadd8(fine >> 1, mid >> 2));

        leds[i] = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
    }

    // Beat: half-intensity white swell — subtle against the slow organic motion.
    if (beat.isActive()) {
        uint8_t flash = beat.flashBrightness(80);
        if (flash > 0)
            for (auto& led : leds) led = blend(led, CRGB::White, flash >> 1);
    }
}

} // namespace Scarfnet
