#include "patterns.h"

namespace Scarfnet {

// Multiple simultaneous firework tracks, each covering its own sub-section of the strip.
//
// Each slot has a session-fixed start/end LED range, period, and noise-space color seed
// derived from ctx.rnd — so the whole fleet sees the same configuration each press.
//
// Without beat: each slot runs at its own autonomous period with a staggered phase
// offset so they don't all launch simultaneously.
//
// With beat: each slot's launch cycle is locked to a different beat multiple (1, 2, 3)
// so their cadences are coprime and they drift in and out of phase naturally.
// Both the launch (frac=0) and arrival (frac=255) are quantized to beat onsets.

static float easeOutQuart(float t) {
    t = 1.0f - t;
    return 1.0f - t * t * t * t;
}

void firework(Leds& leds, const PatternContext& ctx)
{
    constexpr uint8_t  kNumSlots      = 3;    // simultaneous firework tracks
    constexpr uint8_t  kFadeAmount    = 16;   // trail decay per frame; higher = shorter trails
    constexpr uint8_t  kSpanMin       = 6;    // minimum travel span (LEDs)
    constexpr uint8_t  kPeriodMinHd   = 10;   // min autonomous period (×100 ms = 1 s)
    constexpr uint8_t  kPeriodMaxHd   = 80;   // max autonomous period (×100 ms = 8 s)
    constexpr uint16_t kNoiseYScale   = 30;   // noise Y step per LED for color variation
    constexpr uint8_t  kNoiseSlotStep = 100;  // noise X offset between slots for distinct colors

    const uint32_t tMs  = (uint32_t)ctx.timeMs;
    const int16_t  dist = (int16_t)(tMs >> 3);  // drifts over time so each launch has a fresh color

    fadeToBlackBy(leds.data(), kNumLeds, kFadeAmount);

    for (uint8_t s = 0; s < kNumSlots; ++s) {
        // Per-slot seeds: fleet-wide (ctx.rnd) sets the shared character;
        // localSlotRnd mixes in ctx.localRnd for per-device variation.
        const Rnd slotRnd      = (Rnd)(ctx.rnd      * (uint16_t)(s * 137u + 59u));
        const Rnd slotRndB     = (Rnd)(slotRnd       * 211u ^ ctx.rnd);
        const Rnd localSlotRnd = (Rnd)(ctx.localRnd ^ (uint16_t)(s * 97u + 31u));

        // Sub-section: fleet seed sets the base center; localRnd nudges it ±4 LEDs
        // so scarves cover slightly different sub-ranges of the strip.
        const int8_t  localNudge = (int8_t)((uint8_t)localSlotRnd % 9) - 4;  // −4..+4
        const uint8_t center = (uint8_t)max((int)kSpanMin,
                                 min((int)(kNumLeds - kSpanMin),
                                     (int)rndRange(slotRnd, kSpanMin, kNumLeds - kSpanMin) + localNudge));
        const uint8_t halfSpan = rndRange(slotRndB, kSpanMin / 2, (kNumLeds - kSpanMin) / 2 + 1);
        const uint8_t startLed = (center > halfSpan) ? center - halfSpan : 0;
        const uint8_t endLed   = (uint8_t)min((int)center + (int)halfSpan, kNumLeds - 1);
        const uint8_t span     = endLed - startLed;
        if (span < kSpanMin) continue;

        // Phase fraction [0..255]: 0 = launch end, 255 = arrival end
        uint8_t frac;
        if (ctx.beat.isActive()) {
            // Beat-locked: slots use 1-, 2-, 3-beat cycles so they stay independent.
            // Both launch and arrival align to beat onsets.
            const uint8_t  beatMult   = s + 1;  // 1, 2, 3 — coprime cadences
            const uint32_t periodMs   = (uint32_t)ctx.beat.intervalMs * beatMult;
            const uint32_t cyclePosMs = (uint32_t)(ctx.beat.beatNumber % beatMult)
                                      * ctx.beat.intervalMs + ctx.beat.phaseMs;
            frac = (uint8_t)((uint64_t)cyclePosMs * 255u / periodMs);
        } else {
            // Autonomous: each slot has its own period and a fixed phase offset
            // so they don't all launch simultaneously.
            const uint32_t periodMs   = (uint32_t)rndRange(slotRnd,  kPeriodMinHd, kPeriodMaxHd) * 100u;
            const uint32_t phaseOffMs = (uint32_t)slotRndB % periodMs;
            frac = timeFrac8(tMs + phaseOffMs, periodMs);
        }

        // Ease and map to LED position within this slot's span.
        // Direction is session-fixed per slot: bit 0 of slotRndB picks low→high or high→low.
        const float   eased    = easeOutQuart((float)frac / 255.0f);
        // Direction: fleet seed and local seed both contribute — XOR of their low bits
        // means ~50% of devices will travel the opposite direction from the fleet default.
        const bool    reversed = ((slotRndB ^ localSlotRnd) & 1);
        const uint8_t ledPos   = reversed
            ? endLed   - (uint8_t)(eased * (float)span)
            : startLed + (uint8_t)(eased * (float)span);

        // Color: noise in slot-offset X so each slot has a distinct palette hue trajectory
        const uint8_t colorIdx = inoise8((uint16_t)(s * kNoiseSlotStep),
                                         (uint16_t)((uint16_t)dist + (uint16_t)(ledPos * kNoiseYScale))) % 255;
        leds[ledPos] = ColorFromPalette(ctx.palette, colorIdx, 255, LINEARBLEND);
        leds[ledPos].maximizeBrightness();
    }
}

} // namespace Scarfnet
