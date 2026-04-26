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
             const BeatInfo& beat, uint8_t spatialScale, Rnd rnd)
{
    // Every phrase, speed up the noise traversal to create a sense of phrasing.
    constexpr uint8_t  kPhraseLength       = 4;    // beats per phrase for the speed burst
    constexpr uint8_t  kPhraseSpeedMult    = 3;    // how many times faster on the last beat of a phrase
    // Noise time divisors per octave — larger = slower drift
    constexpr uint32_t kFineTimeDivisor    = 4;    // fast shimmer  (~0.5 s per noise unit)
    constexpr uint32_t kMidTimeDivisor     = 50;   // rolling structure (~10 s per noise unit)
    constexpr uint32_t kCoarseTimeDivisor  = 1000; // slow macro drift (~4 min per noise unit)
    // Brightness range
    constexpr uint8_t  kMinBrightness      = 160;  // darkest pixel (noise troughs)
    constexpr uint8_t  kMaxBrightness      = 255;  // brightest pixel (noise peaks)

    uint8_t speedMult = 1;
    if (beat.isActive() && beat.intervalMs > 0) {
        uint32_t beatCount = (uint32_t)((uint32_t)timeMs / beat.intervalMs);
        if ((beatCount % kPhraseLength) >= kPhraseLength - 1) {
            speedMult = kPhraseSpeedMult;
        }
    }

    const uint32_t t = (uint32_t)timeMs * speedMult;

    for (int i = 0; i < kNumLeds; ++i) {
        const uint8_t x = (uint8_t)(i * spatialScale);

        const uint8_t fine   = inoise8(x,       t / kFineTimeDivisor);
        const uint8_t mid    = inoise8(x + 100, t / kMidTimeDivisor);
        const uint8_t coarse = inoise8(x + 200, t / kCoarseTimeDivisor);

        // fBm blend: coarser octave drives most of the hue so the slow drift
        // is clearly visible; finer octaves contribute detail and shimmer.
        uint8_t hue        = (coarse >> 2) + (mid >> 1) + (fine >> 1);
        uint8_t brightness = lerp8by8(kMinBrightness, kMaxBrightness, qadd8(fine >> 1, mid >> 2));

        leds[i] = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
    }

    // Beat: colored flash whose intensity follows each LED's current luma —
    // the beat lights up the noise peaks and leaves the valleys dim, reinforcing
    // the fractal structure instead of washing it out with uniform white.
    if (beat.isActive()) {
        constexpr uint16_t kBeatFlashMs = 100;  // beat flash decay window (ms)
        uint8_t flash = beat.sawTime(kBeatFlashMs);
        if (flash > 0) {
            uint8_t flashHue = (uint8_t)(rnd * 17u >> 8);
            CRGB flashColor = ColorFromPalette(palette, flashHue, 255, LINEARBLEND);
            for (auto& led : leds)
                led = blend(led, flashColor, scale8(flash, led.getLuma()));
        }
    }
}

} // namespace Scarfnet
