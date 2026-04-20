#include "patterns.h"

namespace Scarfnet {

// Dim palette-tinted base with rapid white sparks that decay quickly.
//
// This pattern carries state in the leds array between frames — the fast
// fadeToBlack each tick creates the decay without static variables.
// It converges to near-black within a few frames after a pattern switch, so
// visual transition artifacts are minimal.
//
// `sparkleRate` — probability [0, 255] that any given LED pops each frame.
//               Higher = denser. Derive from rnd in the lambda.
void sparkle(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
             uint8_t sparkleRate)
{
    // Decay: pull each LED quickly toward black
    fadeToBlackBy(leds.data(), kNumLeds, 80);

    // Blend in a dim palette base so the strip isn't purely sparks on black
    uint8_t hue = (uint8_t)((uint32_t)timeMs / 100);
    CRGB baseColor = ColorFromPalette(palette, hue, 55, LINEARBLEND);
    for (auto& led : leds) led = blend(led, baseColor, 25);

    // White sparks
    for (int i = 0; i < kNumLeds; ++i) {
        if (random8() < sparkleRate)
            leds[i] = CRGB::White;
    }
}

} // namespace Scarfnet
