#include "patterns.h"

namespace Scarfnet {

// Beat-primary pattern that reacts strongly to tap-tempo.
//
// Without a beat: a slow autonomous sine-wave pulse (graceful fallback).
//
// With a beat:
//   - The whole strip pulses in brightness on each beat.
//   - On phrase boundaries (every 8 beats) the strip flashes white and
//     scatters bright sparks across the leds array.
//
// Phrase detection uses the beat number derived from mesh time and beat phase,
// so all scarves identify phrase starts identically.
void dance(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
           const BeatInfo& beat, uint8_t hueSpeed)
{
    uint8_t hue = (uint8_t)((uint32_t)timeMs / hueSpeed);

    if (!beat.isActive()) {
        // Fallback: autonomous 4-second breathing pulse
        uint16_t angle = (uint16_t)((uint32_t)(timeMs % 4000) * 65536UL / 4000UL);
        uint8_t brightness = lerp8by8(40, 190, (uint8_t)((sin16(angle) + 32767) >> 8));
        CRGB color = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
        for (auto& led : leds) led = color;
        return;
    }

    // Beat-driven brightness: peak on the beat, dim between
    uint8_t flash      = beat.flashBrightness(beat.intervalMs / 2);
    uint8_t brightness = lerp8by8(50, 255, flash);
    CRGB color = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
    for (auto& led : leds) led = color;

    // Phrase boundary: every 8 beats — white flash + spark scatter
    uint16_t beatNum     = (uint16_t)((timeMs - (int32_t)beat.phaseMs) / beat.intervalMs);
    bool     phraseStart = (beatNum % 8 == 0) && beat.phaseMs < 80;
    if (phraseStart) {
        for (auto& led : leds) led = blend(led, CRGB::White, 200);
        // Scatter a burst of bright palette sparks for extra visual pop
        for (int i = 0; i < kNumLeds / 3; ++i)
            leds[random8(kNumLeds)] = ColorFromPalette(palette, hue + random8(64), 255, LINEARBLEND);
    }
}

} // namespace Scarfnet
