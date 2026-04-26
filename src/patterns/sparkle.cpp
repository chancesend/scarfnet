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
    constexpr uint8_t  kFadeAmount      = 30;   // per-frame decay; higher = shorter trails
    constexpr uint32_t kHueDriftDivisor = 150;  // larger = slower hue drift through palette
    constexpr uint8_t  kBaseBrightness  = 35;   // dim glow between sparks
    constexpr uint8_t  kBaseBlendAmt    = 18;   // how strongly base color blends each frame
    constexpr uint8_t  kWhiteChance     = 42;   // out of 255; ~1-in-6 sparks are white
    constexpr uint8_t  kHueSpread       = 48;   // hue variation range around base hue
    constexpr uint8_t  kSparkBrightness = 220;  // brightness of palette-colored sparks

    // Slow decay for delicate, lingering trails
    fadeToBlackBy(leds.data(), kNumLeds, kFadeAmount);

    // Dim palette base — keeps the strip from going fully dark between sparks
    uint8_t hue = (uint8_t)((uint32_t)timeMs / kHueDriftDivisor);
    CRGB baseColor = ColorFromPalette(palette, hue, kBaseBrightness, LINEARBLEND);
    for (auto& led : leds) led = blend(led, baseColor, kBaseBlendAmt);

    // Mostly palette-colored sparks; occasional white for a natural twinkle mix
    for (int i = 0; i < kNumLeds; ++i) {
        if (random8() < sparkleRate) {
            if (random8() < kWhiteChance) {
                leds[i] = CRGB::White;
            } else {
                uint8_t sparkHue = hue + random8(kHueSpread);
                leds[i] = ColorFromPalette(palette, sparkHue, kSparkBrightness, LINEARBLEND);
            }
        }
    }
}

} // namespace Scarfnet
