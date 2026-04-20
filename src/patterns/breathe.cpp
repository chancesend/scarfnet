#include "patterns.h"

namespace Scarfnet {

// Whole strip inhales and exhales as one — a slow sine-wave brightness pulse.
// The palette color drifts gradually so successive breaths shift in hue.
// On beat: a white flash rides on top of the natural pulse.
//
// `periodMs`  — length of one breath cycle. Varies per fleet-press via rnd.
// `hueSpeed`  — divisor for timeMs → hue mapping; larger = slower color drift.
void breathe(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
             const BeatInfo& beat, int32_t periodMs, uint8_t hueSpeed)
{
    // Map time into a 16-bit angle for sin16 (full cycle = 65536)
    uint16_t angle = (uint16_t)((uint32_t)(timeMs % periodMs) * 65536UL / (uint32_t)periodMs);
    // sin16 → [−32767, 32767] → remap to brightness [30, 220]
    uint8_t brightness = lerp8by8(30, 220, (uint8_t)((sin16(angle) + 32767) >> 8));

    // Slowly drift hue through the palette
    uint8_t hue = (uint8_t)((uint32_t)timeMs / hueSpeed);

    CRGB color = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
    for (auto& led : leds) led = color;

    // Beat: white flash blended over the natural pulse
    if (beat.isActive()) {
        uint8_t flash = beat.flashBrightness(100);
        if (flash > 0)
            for (auto& led : leds) led = blend(led, CRGB::White, flash >> 1);
    }
}

} // namespace Scarfnet
