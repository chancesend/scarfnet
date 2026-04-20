#include "patterns.h"

namespace Scarfnet {

void colorwaves(Leds& leds, int32_t timeMs, const CRGBPalette16& palette)
{
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;
    uint16_t hueinc16 = beatsin88(113, 300, 1500);

    uint16_t ms = timeMs;
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for (uint16_t i = 0; i < leds.size(); i++) {
        hue16 += hueinc16;
        uint16_t h16_128 = hue16 >> 7;
        uint8_t hue8 = (h16_128 & 0x100) ? 255 - (h16_128 >> 1) : h16_128 >> 1;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        uint8_t index = scale8(hue8, 240);
        CRGB newcolor = ColorFromPalette(palette, index, bri8);

        uint16_t pixelnumber = (leds.size() - 1) - i;
        nblend(leds[pixelnumber], newcolor, 128);
    }
}

} // namespace Scarfnet
