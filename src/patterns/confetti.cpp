#include "patterns.h"

namespace Scarfnet {

void confetti(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, uint8_t fade, uint8_t popChancePct)
{
    static int thishue = 150;

    constexpr uint8_t thisinc  = 20;   // hue rotation step
    constexpr uint8_t thissat  = 255;
    constexpr uint8_t thisbri  = 200;
    constexpr int     huediff  = 100;

    fadeToBlackBy(leds.data(), kNumLeds, fade);
    int pos = random16(kNumLeds);
    bool doPop = random16(100) > (100 - popChancePct);
    if (doPop) {
        leds[pos] = ColorFromPalette(palette, thishue + random16(huediff) / 4,
                                     thisbri, LINEARBLEND_NOWRAP);
    }
    thishue += thisinc;
}

} // namespace Scarfnet
