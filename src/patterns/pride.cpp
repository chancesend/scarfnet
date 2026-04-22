#include "patterns.h"

namespace Scarfnet {

void pride(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat)
{
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = timeMs;
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;

    // Phrase-end fill: 5× speed for the last beat of every 16-beat phrase.
    // Builds tension into the phrase boundary without touching anything visual directly.
    const uint8_t speedMult = ((beat.beatNumber % 16) == 15) ? 5 : 1;

    sPseudotime += deltams * msmultiplier * speedMult;
    sHue16 += deltams * beatsin88(400, 5, 9) * speedMult;
    uint16_t brightnesstheta16 = sPseudotime;

    for (uint16_t i = 0; i < kNumLeds; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = ColorFromPalette(palette, hue8, bri8, LINEARBLEND);

        uint16_t pixelnumber = (kNumLeds - 1) - i;
        nblend(leds[pixelnumber], newcolor, 64);
    }

    // On-beat white flash: blend all LEDs toward white, fading over ~60ms.
    if (beat.isActive()) {
        uint8_t flash = beat.flashBrightness();
        if (flash > 0) {
            for (auto& led : leds)
                led = blend(led, CRGB::White, flash / 2);
        }
    }
}

} // namespace Scarfnet
