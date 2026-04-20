#include "patterns.h"

namespace Scarfnet {

// Beat-primary pattern that reacts strongly to tap-tempo.
//
// Without a beat: a slow autonomous sine-wave pulse (graceful fallback).
//
// With a beat, three layers compose on top of each other:
//
//   1. Beat brightness pulse — whole strip peaks on each beat.
//
//   2. Cylon underlay — a single sweep over 2 beats at low blend.
//      It's most visible between beats and nearly disappears on the beat flash,
//      giving the pattern horizontal motion without fighting the vertical pulse.
//
//   3. Phrase events:
//        Every 8 beats   — white flash + spark scatter
//        Every 32/64 beats (macroPeriod, from rnd) — "wild window" for the
//          first 4 beats: two fast overlapping cylon sweeps at high intensity,
//          sparks on every beat onset, brightness pegged to full.
//
// Phrase detection uses the beat number derived from mesh time and beat phase,
// so all scarves identify phrase starts identically.

void dance(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
           const BeatInfo& beat, uint8_t hueSpeed, uint8_t macroPeriod)
{
    const uint8_t hue = (uint8_t)((uint32_t)timeMs / hueSpeed);

    if (!beat.isActive()) {
        // Fallback: autonomous 4-second breathing pulse
        uint16_t angle = (uint16_t)((uint32_t)(timeMs % 4000) * 65536UL / 4000UL);
        uint8_t brightness = lerp8by8(40, 190, (uint8_t)((sin16(angle) + 32767) >> 8));
        CRGB color = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
        for (auto& led : leds) led = color;
        return;
    }

    const uint16_t beatNum     = (uint16_t)((timeMs - (int32_t)beat.phaseMs) / beat.intervalMs);
    const bool     wildWindow  = (beatNum % macroPeriod) < 2;  // first 2 beats of macro phrase
    const uint8_t  flash       = beat.flashBrightness(beat.intervalMs / 2);

    // ── Layer 1: Beat brightness pulse ────────────────────────────────────────
    uint8_t brightness = wildWindow ? 255 : lerp8by8(50, 255, flash);
    CRGB color = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
    for (auto& led : leds) led = color;

    // ── Layer 2: Cylon underlay ───────────────────────────────────────────────
    // Normal: single sweep over 2 beats, blend inversely tracks the beat flash
    //   so the cylon ghosts in between beats and nearly vanishes on the downbeat.
    // Wild:   two sweeps at half-beat speed, high intensity.
    {
        int      sweepCount = wildWindow ? 2 : 1;
        uint32_t period     = wildWindow ? beat.intervalMs / 2
                                         : (uint32_t)beat.intervalMs * 2;
        uint8_t  width      = wildWindow ? 3 : 5;

        for (int s = 0; s < sweepCount; ++s) {
            uint32_t tOff = ((uint32_t)timeMs + (uint32_t)s * period / sweepCount) % period;
            uint8_t  frac = (uint8_t)(tOff * 255 / period);
            uint8_t  pos  = (uint8_t)((uint16_t)quadwave8(frac) * kNumLeds >> 8);
            uint8_t  cylHue = hue + (uint8_t)(s * 128);  // second sweep is palette-complementary
            CRGB cylColor = ColorFromPalette(palette, cylHue, 220, LINEARBLEND);
            // Normal: fade from 50→10 as flash grows (cylon dims on beat)
            uint8_t maxAmt = wildWindow ? 220 : lerp8by8(50, 10, flash);

            for (int i = 0; i < kNumLeds; ++i) {
                uint8_t dist = (uint8_t)abs(i - (int)pos);
                if (dist < width) {
                    uint8_t amt = lerp8by8(maxAmt, 0, (uint8_t)(dist * 255 / width));
                    leds[i] = blend(leds[i], cylColor, amt);
                }
            }
        }
    }

    // ── Layer 3: Phrase events ────────────────────────────────────────────────
    const bool onBeatOnset = beat.phaseMs < 80;

    if (wildWindow) {
        // Wild window: sparks fire on every beat onset for extra chaos
        if (onBeatOnset) {
            for (int i = 0; i < kNumLeds / 2; ++i)
                leds[random8(kNumLeds)] = ColorFromPalette(palette, hue + random8(128), 255, LINEARBLEND);
        }
    } else {
        // Regular phrase boundary every 8 beats: white flash + spark scatter
        bool phraseStart = (beatNum % 8 == 0) && onBeatOnset;
        if (phraseStart) {
            for (auto& led : leds) led = blend(led, CRGB::White, 200);
            for (int i = 0; i < kNumLeds / 3; ++i)
                leds[random8(kNumLeds)] = ColorFromPalette(palette, hue + random8(64), 255, LINEARBLEND);
        }
    }
}

} // namespace Scarfnet
