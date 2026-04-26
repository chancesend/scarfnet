#include "patterns.h"

namespace Scarfnet {

// Whole strip inhales and exhales as one — a slow sine-wave brightness pulse.
// The palette color drifts gradually so successive breaths shift in hue.
// On beat: a white flash rides on top of the natural pulse.
//
// `periodMs`  — length of one breath cycle. Varies per fleet-press via rnd.
// `hueSpeed`  — divisor for timeMs → hue mapping; larger = slower color drift.
// `rnd`       — fleet seed; used to tint the beat gasp color.
void breathe(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
             const BeatInfo& beat, int32_t periodMs, uint8_t hueSpeed, Rnd rnd)
{
    // Map time into a 16-bit angle for sin16 (full cycle = 65536)
    uint16_t angle = (uint16_t)((uint32_t)(timeMs % periodMs) * 65536UL / (uint32_t)periodMs);
    constexpr uint8_t kMinBrightness  = 30;   // dim end of breath (inhale)
    constexpr uint8_t kMaxBrightness  = 220;  // bright end of breath (exhale)

    // sin16 → [−32767, 32767] → remap to brightness range
    uint8_t brightness = lerp8by8(kMinBrightness, kMaxBrightness, (uint8_t)((sin16(angle) + 32767) >> 8));

    // Slowly drift hue through the palette (hoisted so beat block can reference it)
    const uint8_t hue = (uint8_t)((uint32_t)timeMs / hueSpeed);

    CRGB color = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
    for (auto& led : leds) led = color;

    // Beat: a sharp "gasp" — a contrasting palette color that snaps brighter than
    // the natural sine peak, then vanishes quickly. Wider window and full
    // blend amount make it feel like a caught breath against the slow rhythm.
    if (beat.isActive()) {
        constexpr uint16_t kBeatFlashMs    = 150;  // decay window for beat gasp (ms); shorter = snappier
        constexpr uint8_t  kFlashHueOffset = 96;   // palette offset for the flash vs breath color (~quarter turn)
        uint8_t flash = beat.sawTime(kBeatFlashMs);
        if (flash > 0) {
            uint8_t flashHue = hue + kFlashHueOffset + (uint8_t)(rnd * 23u >> 8);
            CRGB flashColor = ColorFromPalette(palette, flashHue, 255, LINEARBLEND);
            for (auto& led : leds) led = blend(led, flashColor, flash);
        }
    }
}

} // namespace Scarfnet
