#include "patterns.h"

namespace Scarfnet {

void pride(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat)
{
    // All animation parameters derived from timeMs — fully stateless.
    // beatsinT replaces FastLED's beatsin88 (which uses millis()) with a timeMs-based
    // equivalent so all scarves stay phase-locked on the same mesh time.
    auto beatsinT = [timeMs](uint16_t bpm, uint16_t lo, uint16_t hi) -> uint16_t {
        uint16_t phase = (uint16_t)((uint64_t)(uint32_t)timeMs * bpm * 65536 / 60000);
        return lo + (uint16_t)((uint32_t)(hi - lo) * (uint16_t)((int32_t)sin16(phase) + 32768) / 65536);
    };

    uint8_t  brightdepth        = (uint8_t)beatsinT(341, 96, 224);
    uint16_t brightnessthetainc = beatsinT(203, 25 * 256, 40 * 256);
    uint16_t hueinc16           = beatsinT(113, 1, 3000);

    // Pseudotime: time-integral of msmultiplier (beatsin88(147, 23, 60), avg ≈ 41/ms).
    // Linear approximation at the average rate — wraps naturally as uint16_t.
    uint16_t pseudotime = (uint16_t)((uint32_t)timeMs * 41u);

    // sHue16: time-integral of hue scroll rate (beatsin88(400, 5, 9), avg ≈ 7/ms).
    uint16_t hue16 = (uint16_t)((uint32_t)timeMs * 7u);

    for (uint16_t i = 0; i < kNumLeds; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        uint16_t brightnesstheta = pseudotime + (uint16_t)((uint32_t)(i + 1) * brightnessthetainc);
        uint16_t b16  = sin16(brightnesstheta) + 32768;
        uint16_t bri16 = (uint32_t)b16 * b16 / 65536;
        uint8_t  bri8  = (uint32_t)bri16 * brightdepth / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = ColorFromPalette(palette, hue8, bri8, LINEARBLEND);
        nblend(leds[(kNumLeds - 1) - i], newcolor, 64);
    }

    // On-beat white flash: fades out over ~60ms.
    if (beat.isActive()) {
        uint8_t flash = beat.sawTime();
        if (flash > 0)
            for (auto& led : leds) led = blend(led, CRGB::White, flash / 2);
    }
}

} // namespace Scarfnet
