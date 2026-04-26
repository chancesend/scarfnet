#include "patterns.h"

namespace Scarfnet {

// Beat-primary pattern that reacts strongly to tap-tempo.
//
// Without tap-tempo: a synthetic ~20 BPM (3000 ms) beat drives layers 1 and 2
// for a slow, meditative pulse + cylon underlay. Phrase/wild effects are
// suppressed — the slow beat gives calm, continuous motion.
//
// With tap-tempo, three layers compose:
//
//   1. Beat brightness pulse — peaks at a per-beat center LED, linear falloff.
//      Center derived from beatNum so all scarves land on the same LED.
//
//   2. Cylon underlay — single sweep over 2 beats at low blend.
//      Ghosts in between beats, nearly vanishes on the downbeat.
//
//   3. Phrase events (tap-tempo only):
//        Every 8 beats   — white flash + spark scatter
//        Every 32/64 beats (macroPeriod) — "wild window" for the first 2 beats:
//          two fast overlapping cylon sweeps at high intensity + sparks on onset.

void dance(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
           const BeatInfo& beat, uint8_t hueSpeed, uint8_t macroPeriod, Rnd localRnd)
{
    const uint8_t hue = (uint8_t)((uint32_t)timeMs / hueSpeed);

    // When no tap-tempo is active, substitute a synthetic slow beat (~20 BPM).
    // Phrase/wild effects are suppressed in this mode.
    constexpr uint16_t kSlowBeatMs = 3000;
    BeatInfo slowBeat {
        kSlowBeatMs,
        (uint16_t)((uint32_t)timeMs % kSlowBeatMs),
        (uint16_t)((uint32_t)timeMs / kSlowBeatMs)
    };
    const BeatInfo& active = beat.isActive() ? beat : slowBeat;

    const uint16_t beatNum    = (uint16_t)((timeMs - (int32_t)active.phaseMs) / active.intervalMs);
    // wildWindow only applies when tap-tempo is live
    const bool     wildWindow = beat.isActive() && (beatNum % macroPeriod) < 2;
    const uint8_t  flash      = active.sawTime(active.intervalMs / 2);

    // ── Layer 1: Beat brightness pulse with radial falloff ────────────────────
    const int kFalloffRadius = kNumLeds / 2;
    int center = (int)((uint32_t)((beatNum ^ (uint32_t)localRnd) * 2654435761u) >> 27) % kNumLeds;
    uint8_t peakBrightness = wildWindow ? 255 : lerp8by8(90, 255, flash);

    for (int i = 0; i < kNumLeds; ++i) {
        int dist = abs(i - center);
        uint8_t localBright = (dist >= kFalloffRadius)
            ? 0
            : (uint8_t)((uint32_t)peakBrightness * (uint32_t)(kFalloffRadius - dist)
                        / (uint32_t)kFalloffRadius);
        leds[i] = ColorFromPalette(palette, hue, localBright, LINEARBLEND);
    }

    // ── Layer 2: Cylon underlay ───────────────────────────────────────────────
    {
        int      sweepCount = wildWindow ? 2 : 1;
        // Non-beat: 8× the (slow) interval for a very gradual underlay sweep
        uint32_t period     = wildWindow ? active.intervalMs / 2
                                         : (uint32_t)active.intervalMs * (beat.isActive() ? 2 : 8);
        uint8_t  width      = wildWindow ? 3 : 5;

        for (int s = 0; s < sweepCount; ++s) {
            uint32_t tOff = ((uint32_t)timeMs + (uint32_t)s * period / sweepCount) % period;
            uint8_t  frac = (uint8_t)(tOff * 255 / period);
            uint8_t  pos  = (uint8_t)((uint16_t)quadwave8(frac) * kNumLeds >> 8);
            constexpr uint8_t kCylonBrightness = 220;  // peak brightness of the cylon beam
            uint8_t  cylHue = hue + (uint8_t)(s * 128);
            CRGB cylColor = ColorFromPalette(palette, cylHue, kCylonBrightness, LINEARBLEND);
            uint8_t maxAmt = wildWindow ? kCylonBrightness : lerp8by8(50, 10, flash);

            for (int i = 0; i < kNumLeds; ++i) {
                uint8_t dist = (uint8_t)abs(i - (int)pos);
                if (dist < width) {
                    uint8_t amt = lerp8by8(maxAmt, 0, (uint8_t)(dist * 255 / width));
                    leds[i] = blend(leds[i], cylColor, amt);
                }
            }
        }
    }

    // ── Layer 3: Phrase events (tap-tempo only) ───────────────────────────────
    if (beat.isActive()) {
        constexpr uint16_t kBeatOnsetWindowMs = 80;  // ms at the top of each beat considered "on onset"
        const bool onBeatOnset = active.phaseMs < kBeatOnsetWindowMs;

        if (wildWindow) {
            if (onBeatOnset) {
                for (int i = 0; i < kNumLeds / 2; ++i)
                    leds[random8(kNumLeds)] = ColorFromPalette(palette, hue + random8(128), 255, LINEARBLEND);
            }
        } else {
            if ((beatNum % 8 == 0) && onBeatOnset) {
                for (int i = 0; i < 4; ++i)
                    leds[random8(kNumLeds)] = CRGB::White;
            }
        }
    }
}

} // namespace Scarfnet
