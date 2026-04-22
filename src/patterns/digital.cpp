#include "patterns.h"

namespace Scarfnet {

void digital(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
             const PatternContext& ctx)
{
    for (int i = 0; i < kNumLeds; ++i) {
        uint8_t hue        = 100 + (inoise8(i * 10, timeMs / 4) >> 2);
        uint8_t brightness = lerp8by8(160, 255, qadd8(inoise8(i * 10 + 100, timeMs / 50) >> 2, inoise8(i * 10 + 200, timeMs / 1000) >> 3));

        if (ctx.beat.isOnBeat(100)) {
            hue += rndRange(ctx.localRnd xor ctx.localRnd, 10, 50);
        }
        if (i % 8 == ctx.beat.barNumber() % 8) {
            brightness = 255;
        }
        leds[i] = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
    }
}

} // namespace Scarfnet
