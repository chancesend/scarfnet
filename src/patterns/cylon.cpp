#include "patterns.h"

namespace Scarfnet {

void cylon(Leds& leds, int32_t timeMs, CRGB color, int width, int periodMs, fract8 blurAmount)
{
    const auto timeFrac  = timeFrac8(timeMs, periodMs);
    const auto cylonFrac = quadwave8(timeFrac);
    const uint8_t lerpVal = lerp8by8(0, leds.size(), cylonFrac);

    for (int i = 0; i <= (int)leds.size(); ++i) {
        const auto startLed = lerpVal - width / 2;
        const auto stopLed  = lerpVal + width / 2;
        leds[i] = (i >= startLed && i <= stopLed) ? color : CRGB::Black;
    }
    blur1d(leds.data(), leds.size(), blurAmount);
}

} // namespace Scarfnet
