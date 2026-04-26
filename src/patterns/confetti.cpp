#include "patterns.h"

namespace Scarfnet {

void confetti(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, uint8_t fade, uint16_t popChancePct)
{
    constexpr uint8_t thissat  = 255;
    constexpr uint8_t thisbri  = 200;
    constexpr int     huediff  = 100;

    // Slowly drifting base hue, derived from timeMs — stateless and cross-scarf locked.
    int thishue = (int)(uint8_t)(timeMs / 60);

    fadeToBlackBy(leds.data(), kNumLeds, fade);
    int popChance = (int)popChancePct;
    do {
        int pos = random16(kNumLeds);
        int popChancePct = std::min(popChance, 100);
        bool doPop = random16(100) > (100 - popChancePct);
        if (doPop) {
            leds[pos] = ColorFromPalette(palette, thishue + random16(huediff) / 4,
                                        thisbri, LINEARBLEND_NOWRAP);
        }
        popChance -= 100;
    } while (popChance > 0);
}

} // namespace Scarfnet
