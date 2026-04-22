#include "patterns.h"

namespace Scarfnet {

static int16_t  dist   = 0;
static const uint16_t yscale = 30;

static float easeOutQuart(float t) {
    return 1 - (--t) * t * t * t;
}

void fillNoise(Leds& leds, int32_t timeMs)
{
    for (int i = 0; i < kNumLeds; i++) {
        uint8_t index = inoise8(0, dist + i * yscale) % 255;
        leds[i] = ColorFromPalette(ForestColors_p, index, 255, LINEARBLEND);
    }
    dist += beatsin8(10, 1, 4, timeMs);
}

void firework(Leds& leds, int32_t timeMs, int periodMs, const CRGBPalette16& palette)
{
    const auto fireworkFrac   = timeFrac8(timeMs, periodMs);
    const uint8_t fireworkEased = easeOutQuart((float)fireworkFrac / 255) * 255;
    const uint8_t fireworkLerpVal = lerp8by8(0, leds.size(), fireworkEased);

    uint8_t index = inoise8(0, dist + fireworkLerpVal * yscale) % 255;
    leds[fireworkLerpVal] = ColorFromPalette(palette, index, 255, LINEARBLEND);
    leds[fireworkLerpVal].maximizeBrightness();

    fadeToBlackBy(leds.data(), leds.size(), 16);
}

} // namespace Scarfnet
