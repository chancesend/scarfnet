#include "patterns.h"

namespace Scarfnet {

constexpr uint16_t kFireworkYScale = 30;

static float easeOutQuart(float t) {
    return 1 - (--t) * t * t * t;
}

void firework(Leds& leds, int32_t timeMs, int periodMs, const CRGBPalette16& palette)
{
    // Noise-space offset: drifts over time so each launch has a distinct color.
    // Original accumulated ~2.5 units/frame via beatsin8(10,1,4); stateless approx: timeMs/8.
    int16_t dist = (int16_t)((uint32_t)timeMs >> 3);

    const auto fireworkFrac      = timeFrac8(timeMs, periodMs);
    const uint8_t fireworkEased  = easeOutQuart((float)fireworkFrac / 255) * 255;
    const uint8_t fireworkLerpVal = lerp8by8(0, leds.size(), fireworkEased);

    uint8_t index = inoise8(0, dist + fireworkLerpVal * kFireworkYScale) % 255;
    leds[fireworkLerpVal] = ColorFromPalette(palette, index, 255, LINEARBLEND);
    leds[fireworkLerpVal].maximizeBrightness();

    constexpr uint8_t kFadeAmount = 16;  // higher = shorter trails

    fadeToBlackBy(leds.data(), leds.size(), kFadeAmount);
}

} // namespace Scarfnet
