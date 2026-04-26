#include "patterns.h"

namespace Scarfnet {

// baseHue: very slowly drifting anchor (~4 min full palette cycle from timeMs >> 10).
// secondaryHue: distinct palette offset, session-fixed — peeks through at the bar center.
// basePeriodMs: beat-locked when tempo active, very slow 30–80 s otherwise.
//   Period drifts ±1.5% over a ~90 s cycle — barely perceptible variation.
//   Main hue oscillates ±12 units around baseHue over a ~20 s cycle.

void cylon(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
           uint8_t baseHue, uint8_t secondaryHue, int width, int basePeriodMs)
{
    // Period drifts ±1.5% over ~90 s — barely perceptible, just enough to feel alive
    constexpr uint32_t kPeriodOscCycleMs = 90000;  // period of the period-drift oscillation
    constexpr uint32_t kHueOscCycleMs    = 20000;  // period of the hue oscillation (~20 s)
    constexpr int8_t   kHueOscAmp        = 12;     // ±hue units around baseHue

    uint16_t periodOscAngle = (uint16_t)((uint32_t)timeMs * 65536u / kPeriodOscCycleMs);
    int32_t  periodOsc      = ((int32_t)sin16(periodOscAngle) * (basePeriodMs >> 6)) >> 15;
    int      period         = max(200, basePeriodMs + (int)periodOsc);

    uint16_t hueOscAngle = (uint16_t)((uint32_t)timeMs * 65536u / kHueOscCycleMs);
    int8_t   hueOsc      = (int8_t)((int32_t)sin16(hueOscAngle) * kHueOscAmp >> 15);
    uint8_t  hue         = baseHue + (uint8_t)hueOsc;

    // Smooth bounce via quadwave8
    const uint8_t frac   = timeFrac8(timeMs, period);
    const int     center = (int)lerp8by8(0, (uint8_t)(kNumLeds - 1), quadwave8(frac));
    const int     halfW  = max(1, width / 2);

    for (int i = 0; i < kNumLeds; ++i) {
        int dist = abs(i - center);
        if (dist > halfW) { leds[i] = CRGB::Black; continue; }

        // t: 0=center, 255=edge. Quadratic falloff for soft, natural edges.
        uint8_t t    = (uint8_t)((uint32_t)dist * 255u / (uint32_t)halfW);
        uint8_t fade = 255 - (uint8_t)(((uint16_t)t * t) >> 8);

        CRGB mainColor = ColorFromPalette(palette, hue, fade, LINEARBLEND);

        // Secondary palette color subtly reflected at the center of the bar
        constexpr uint8_t kSecColorMaxAmt = 40;  // max secondary color at beam center (0=off, 255=full)
        uint8_t secAmt = (uint8_t)((uint16_t)(255 - t) * kSecColorMaxAmt >> 8);
        CRGB secColor  = ColorFromPalette(palette, secondaryHue, 255, LINEARBLEND);

        leds[i] = blend(mainColor, secColor, secAmt);
    }
}

} // namespace Scarfnet
