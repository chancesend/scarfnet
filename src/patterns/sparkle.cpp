#include "patterns.h"

namespace Scarfnet {

// Dim palette-tinted base with occasional colored sparks and rare white flashes.
//
// This pattern carries state in the leds array between frames — the slow
// fadeToBlack each tick creates soft decay trails without static variables.
//
// `sparkleRate` — probability [0, 255] that any given LED pops each frame.
//               Normally kept very low (~1-6); the lambda boosts it for bursts.
void sparkle(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
             uint8_t sparkleRate)
{
    // Slow decay for delicate, lingering trails
    fadeToBlackBy(leds.data(), kNumLeds, 30);

    // Dim palette base — keeps the strip from going fully dark between sparks
    uint8_t hue = (uint8_t)((uint32_t)timeMs / 150);
    CRGB baseColor = ColorFromPalette(palette, hue, 35, LINEARBLEND);
    for (auto& led : leds) led = blend(led, baseColor, 18);

    // Mostly palette-colored sparks; ~1 in 6 are white for a natural twinkle mix
    for (int i = 0; i < kNumLeds; ++i) {
        if (random8() < sparkleRate) {
            if (random8() < 42) {
                leds[i] = CRGB::White;
            } else {
                uint8_t sparkHue = hue + random8(48);  // slight hue spread around base
                leds[i] = ColorFromPalette(palette, sparkHue, 220, LINEARBLEND);
            }
        }
    }
}

} // namespace Scarfnet
