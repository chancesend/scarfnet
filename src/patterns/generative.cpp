#include "patterns.h"

namespace Scarfnet {

// Three-layer generative pattern that evolves its character over ~20 minutes.
//
// Macro vibe signals (shared — pure function of timeMs + rnd, identical on all scarves):
//   vibeFlow   (~10 min cycle) — organic sweep ↔ sharp digital dart
//   vibeEnergy (~6  min cycle) — dim/calm ↔ bright/energetic
//   vibeSplit  (~14 min cycle) — unified ↔ fragmented (digital segments)
//
// rnd offsets all three vibe signals into different noise-field regions, so each
// selection of this pattern starts with a different initial vibe and follows a
// completely different 20-minute trajectory.
//
// Per-device variation (from localRnd, re-seeded on each pattern change):
//   localPhase  — shifts spatial noise lookup so scarves see different regions
//   localOffset — second noise offset for coarse octave (independent texture)
//   localHue    — per-device hue bias (subtle, not jarring)
//
// The result: scarves look distinct frame-to-frame but pulse at the same energy,
// transition to digital mode together, and feel like one organism in aggregate.
//
// Layers:
//   1. Organic noise base (always present)
//   2. Moving streak whose speed and width morph with vibeFlow
//   3. Snapping digital segments that appear when vibeSplit is high

void generative(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
                const BeatInfo& beat, Rnd rnd, Rnd localRnd)
{
    const uint32_t t = (uint32_t)timeMs;

    // ── Fleet-wide seed from rnd ───────────────────────────────────────────────
    // Offsets the three vibe signals into independent noise-field regions so each
    // button press puts the whole fleet on a different 20-minute trajectory.
    const uint8_t rndA = (uint8_t)rnd;          // vibe noise Y-offset for flow signal
    const uint8_t rndB = (uint8_t)(rnd >> 5);   // vibe noise Y-offset for energy signal
    const uint8_t rndC = (uint8_t)(rnd >> 10);  // vibe noise Y-offset for split signal
    const uint8_t rndHue = (uint8_t)(rnd * 17u >> 8);  // fleet-wide hue rotation [0, ~255]

    // ── Per-device constants seeded from localRnd ─────────────────────────────
    const uint8_t  localPhase  = (uint8_t)localRnd;              // spatial noise shift
    const uint8_t  localOffset = (uint8_t)(localRnd >> 8);       // second noise domain offset
    const uint8_t  localHue    = (uint8_t)(localRnd * 11u >> 8); // subtle hue bias [0, ~43]

    // ── Slow macro vibe signals (shared across all scarves) ───────────────────
    // Cycles are incommensurate so the combined vibe space doesn't repeat within 20 min.
    // rndA/B/C shift each signal to a different starting region of the noise field.
    const uint8_t vibeFlow   = inoise8(t / 2500, rndA);          // ~10 min full cycle
    const uint8_t vibeEnergy = inoise8(t / 1500, 100 + rndB);    //  ~6 min full cycle
    const uint8_t vibeSplit  = inoise8(t / 3500, 200 + rndC);    // ~14 min full cycle

    // ── Layer 1: Organic noise base ───────────────────────────────────────────
    // Hue is driven by a shared coarse signal (no localOffset) so all scarves
    // stay in the same color zone. Per-device offsets only affect fine texture
    // and brightness, giving each scarf a different shimmer pattern within the
    // shared palette neighborhood.
    for (int i = 0; i < kNumLeds; ++i) {
        const uint8_t x = (uint8_t)(i * 8 + localPhase);

        // Shared hue anchor — no localOffset, identical across the fleet
        const uint8_t sharedCoarse = inoise8(x, t / 600);

        // Per-device texture — shifts each scarf's brightness pattern independently
        const uint8_t fine   = inoise8(x + localOffset,       t / 40);   // fast shimmer
        const uint8_t coarse = inoise8(x + localOffset + 100, t / 600);  // brightness roll

        // sharedCoarse dominates hue; fine adds only a small local shimmer-shift [0..15]
        uint8_t hue = (sharedCoarse >> 1) + (fine >> 4) + (uint8_t)(t / 200) + rndHue + (localHue >> 1);
        // vibeEnergy is shared — all scarves brighten and dim together
        uint8_t bri = lerp8by8(100, qadd8(vibeEnergy, 80), qadd8(fine >> 1, coarse >> 2));

        leds[i] = ColorFromPalette(palette, hue, bri, LINEARBLEND);
    }

    // ── Layer 2: Moving energy streak ─────────────────────────────────────────
    // vibeFlow morphs its character:
    //   low  vibeFlow → wide (8 LEDs), slow (16 s sweep) — organic
    //   high vibeFlow → narrow (2 LEDs), slow-ish (1.5 s dart) — digital
    // localPhase staggers the streak position across scarves so they're not
    // all at the same point simultaneously.
    //
    // Phrase-end burst: on the last beat of every 16-beat phrase the streak
    // overrides to a fast dart (300 ms) at full brightness with a slight white wash.
    {
        const uint8_t streakWidth = lerp8by8(8, 2, vibeFlow);
        const uint32_t period     = (uint32_t)lerp8by8(160, 15, vibeFlow) * 100;  // 16000–1500 ms

        // Detect last beat of every 16-beat phrase for the burst
        // Fades from 255 → 0 over the course of that last beat
        const uint8_t phraseBurst = ((beat.beatNumber % 16) == 15)
            ? beat.saw(255, 0)
            : 0;

        const uint32_t activePeriod = (phraseBurst > 0) ? 300u : period;

        // Apply per-device phase offset to streak position
        uint32_t tOff = (t + (uint32_t)localPhase * (activePeriod >> 8)) % activePeriod;
        uint8_t  frac = (uint8_t)(tOff * 255 / activePeriod);
        uint8_t  pos  = (uint8_t)((uint16_t)quadwave8(frac) * kNumLeds >> 8);

        uint8_t streakHue = (uint8_t)(t / 120 + rndHue + localHue);
        uint8_t streakBri = (phraseBurst > 0) ? 255 : 150;
        CRGB streakColor = ColorFromPalette(palette, streakHue, streakBri, LINEARBLEND);
        if (phraseBurst > 0) {
            streakColor = blend(streakColor, CRGB::White, phraseBurst >> 2);  // subtle whitening at burst peak
        }

        for (int i = 0; i < kNumLeds; ++i) {
            uint8_t dist = (uint8_t)abs(i - (int)pos);
            if (dist < streakWidth) {
                uint8_t blend_amt = (phraseBurst > 0)
                    ? lerp8by8(200, 80, dist * 255 / streakWidth)
                    : lerp8by8(130, 40, dist * 255 / streakWidth);
                leds[i] = blend(leds[i], streakColor, blend_amt);
            }
        }
    }

    // ── Layer 3: Digital fragmentation ────────────────────────────────────────
    // When vibeSplit is high, hard-edge segments snap to new positions on a
    // fixed cadence — a clock-like digital element that interrupts the organic flow.
    // Each scarf snaps to different positions (via localPhase, localRnd).
    //
    // When a beat is active the snap cadence locks to the tempo:
    //   half-beat  (÷2) — snaps on every half-beat, tight rhythmic chop
    //   quarter-beat (÷4) — used when vibeFlow is high (fast/digital character)
    // When no beat, an autonomous period (380–560 ms from localRnd) is used.
    if (vibeSplit > 110) {
        const uint8_t excess  = vibeSplit - 110;                              // 0–145
        const uint8_t fragAmt = (excess > 127) ? 255 : (uint8_t)(excess * 2); // 0→255 as vibeSplit→237

        uint16_t snapPeriod;
        if (beat.isActive() && beat.intervalMs > 0) {
            // Subdivide: quarter-beat when vibeFlow is high (digital mode), else half-beat
            snapPeriod = (vibeFlow > 160) ? beat.intervalMs / 4 : beat.intervalMs / 2;
            snapPeriod = max(snapPeriod, (uint16_t)50);  // floor to avoid degenerate fast-flash
        } else {
            snapPeriod = (uint16_t)(380 + (localRnd & 0xFF) % 180);  // ~380–560 ms autonomous
        }
        const uint16_t snap = (uint16_t)(t / snapPeriod);
        const uint8_t  segStart   = (uint8_t)((snap * 53u + localPhase) % kNumLeds);
        const uint8_t  segLen     = (uint8_t)(2 + (snap * 17u + localOffset) % 5);
        const uint8_t  segHue     = (uint8_t)(snap * 37u + localHue);
        CRGB segColor = ColorFromPalette(palette, segHue, 255, LINEARBLEND);

        for (int i = 0; i < (int)segLen; ++i) {
            int idx = (segStart + i) % kNumLeds;
            leds[idx] = blend(leds[idx], segColor, fragAmt);
        }
    }

    // ── Beat: twinkling scatter burst ─────────────────────────────────────────
    // Random LEDs pop bright in palette colors, densest on the downbeat and
    // sparse at the tail — like sparks thrown by an impact. Each frame during
    // the ~100 ms window fires a different random subset, giving an organic burst.
    if (beat.isActive()) {
        uint8_t flash = beat.sawTime(100);
        if (flash > 0) {
            uint8_t flashHue = (uint8_t)(rnd * 17u >> 8);
            for (int i = 0; i < kNumLeds; ++i) {
                if (random8() < flash) {
                    CRGB pop = ColorFromPalette(palette, flashHue + (uint8_t)(i * 3), 255, LINEARBLEND);
                    leds[i] = blend(leds[i], pop, flash >> 1);
                }
            }
        }
    }
}

} // namespace Scarfnet
