#include "patterns.h"

namespace Scarfnet {

// Step-sequencer pattern designed for tap-tempo.
//
// Without a beat: slow autonomous breathing pulse (minimal fallback).
//
// With a beat, four event layers run through a probabilistic step sequencer.
// Each layer has a session-fixed threshold (from rnd) that gates whether
// the event fires on each opportunity — pressing the button builds a new
// "program" with different events and densities.
//
//   Beat-level (per beat):
//     A. Brightness pulse     — always; sharp sawTime flash over the full strip
//     B. Strobe flash         — fires when strobeRoll < strobeThresh (~25–75%)
//     C. Spark scatter        — fires when sparkRoll  < sparkThresh  (~25–75%)
//     D. Sweep beam           — always; sweeps end-to-end each beat, direction
//                               reverses at every bar boundary, offset per device
//
//   Bar-level (every 4 beats):
//     E. Color wipe           — fires when barHash < barWipeThresh (~25–78%)
//     F. Position marker      — always; white pip marks bar boundary
//
//   Phrase-level (power-of-2 bars):
//     G. 2-bar accent         — fires when barHash < p2Thresh (~31–86%); alternate pixels
//     H. 4-bar inward sweep   — always; bidirectional fill from both ends
//     I. 8-bar white strobe   — always; climax strobe
//     J. 16-bar full burst    — always; peak whitening across all LEDs

void beat(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
          const BeatInfo& b, Rnd rnd, Rnd localRnd)
{
    if (!b.isActive()) {
        // Minimal fallback — this pattern is designed for tap-tempo
        uint16_t angle = (uint16_t)((uint32_t)(timeMs % 4000) * 65536UL / 4000UL);
        uint8_t bright = lerp8by8(20, 100, (uint8_t)((sin16(angle) + 32767) >> 8));
        uint8_t hue = (uint8_t)(timeMs >> 8);
        for (auto& led : leds)
            led = ColorFromPalette(palette, hue, bright, LINEARBLEND);
        return;
    }

    const uint32_t beatNum = b.beatNumber;
    // Hue drifts slowly and steps 40 units at each bar boundary for structure
    const uint8_t hue = (uint8_t)(timeMs >> 8) + (uint8_t)(b.barNumber(4) * 40);

    // ── Session-fixed event thresholds (0–255) ───────────────────────────────
    // Different bits of rnd drive each threshold independently.
    const uint8_t strobeThresh  = rndRange(rnd,        64, 192);  // ~25–75%: beat strobe
    const uint8_t sparkThresh   = rndRange(rnd >> 4,   64, 192);  // ~25–75%: beat sparks
    const uint8_t barWipeThresh = rndRange(rnd >> 8,   64, 200);  // ~25–78%: bar wipe
    const uint8_t p2Thresh      = rndRange(rnd >> 12,  80, 220);  // ~31–86%: 2-bar accent

    // Independent per-beat rolls — different multipliers avoid correlation
    const uint8_t strobeRoll = (uint8_t)(beatNum * 167u ^ (uint8_t)rnd);
    const uint8_t sparkRoll  = (uint8_t)(beatNum * 211u ^ (uint8_t)(rnd >> 4));
    const uint8_t barHash    = (uint8_t)(b.barNumber(4) * 179u ^ (uint8_t)(rnd >> 8));

    // ── Base decay ──────────────────────────────────────────────────────────
    // Aggressive decay keeps inter-beat space dark for a punchy strobe feel
    fadeToBlackBy(leds.data(), kNumLeds, 70);

    // ── A. Beat brightness pulse (always) ───────────────────────────────────
    {
        uint8_t flash = b.sawTime(80);
        if (flash > 0) {
            CRGB c = ColorFromPalette(palette, hue, flash, LINEARBLEND);
            for (auto& led : leds) led = blend(led, c, flash >> 2);
        }
    }

    // ── D. Direction-alternating sweep beam (always) ────────────────────────
    // Sweeps end-to-end each beat; direction reverses at every bar boundary.
    // localRnd staggers the beam phase across devices so scarves don't lock-step.
    {
        uint8_t frac = b.frac8();
        if (b.barNumber(4) & 1) frac = 255 - frac;
        int beamPos = (int)(((uint16_t)frac + (uint8_t)(localRnd >> 8)) * kNumLeds >> 8) % kNumLeds;

        uint8_t beamBright = b.sawTime(b.intervalMs);  // full-beat trail
        CRGB beamColor = ColorFromPalette(palette, hue + 64, beamBright, LINEARBLEND);
        constexpr int kBeamWidth = 3;
        for (int i = 0; i < kNumLeds; ++i) {
            int d = abs(i - beamPos);
            if (d < kBeamWidth) {
                uint8_t amt = lerp8by8(beamBright, 0, (uint8_t)(d * 255 / kBeamWidth));
                leds[i] = blend(leds[i], beamColor, amt);
            }
        }
    }

    const bool onBeat = b.isOnBeat(80);

    // ── B. Beat strobe flash ─────────────────────────────────────────────────
    if (onBeat && strobeRoll < strobeThresh) {
        uint8_t beatHue = hue + (uint8_t)(beatNum * 23u);
        CRGB c = ColorFromPalette(palette, beatHue, 255, LINEARBLEND);
        for (auto& led : leds) led = blend(led, c, 200);
    }

    // ── C. Beat spark scatter ────────────────────────────────────────────────
    if (onBeat && sparkRoll < sparkThresh) {
        uint8_t sparkHue = hue + 128;
        for (int i = 0; i < 8; ++i) {
            // localRnd staggers spark positions across devices
            int pos = (int)((uint32_t)(localRnd + beatNum * (uint32_t)(i * 47u + 13u)) % kNumLeds);
            leds[pos] = ColorFromPalette(palette, sparkHue + (uint8_t)(i * 16), 255, LINEARBLEND);
        }
    }

    // ── E. Bar color wipe ────────────────────────────────────────────────────
    if (b.isOnBar(4, 80) && barHash < barWipeThresh) {
        uint8_t wipeHue = hue + (uint8_t)(b.barNumber(4) * 64);
        uint8_t f = b.sawTime(250);
        CRGB c = ColorFromPalette(palette, wipeHue, f, LINEARBLEND);
        for (auto& led : leds) led = blend(led, c, f >> 1);
    }

    // ── F. Bar position marker (always) ─────────────────────────────────────
    // A single white pip gives the listener a subtle structural anchor.
    if (b.isOnBar(4, 80)) {
        int markerPos = (int)((uint32_t)b.barNumber(4) * 7u % kNumLeds);
        leds[markerPos] = blend(leds[markerPos], CRGB::White, 220);
    }

    // ── G. 2-bar accent (conditional on p2Thresh) ───────────────────────────
    {
        uint8_t f = b.phraseView(8).sawTime(300);
        if (f > 0 && barHash < p2Thresh) {
            CRGB c = ColorFromPalette(palette, hue + 96, f, LINEARBLEND);
            for (int i = 0; i < kNumLeds; i += 2)
                leds[i] = blend(leds[i], c, f >> 1);
        }
    }

    // ── H. 4-bar inward sweep (always) ──────────────────────────────────────
    {
        uint8_t f = b.phraseView(16).sawTime(500);
        if (f > 0) {
            uint8_t phraseHue = hue + (uint8_t)(beatNum * 7u);
            for (int i = 0; i < kNumLeds / 2; ++i) {
                uint8_t dim = (uint8_t)((uint16_t)f * (uint8_t)(kNumLeds / 2 - i) / (kNumLeds / 2));
                CRGB c = ColorFromPalette(palette, phraseHue + (uint8_t)(i * 4), dim, LINEARBLEND);
                leds[i]                  = blend(leds[i],                  c, dim);
                leds[(kNumLeds - 1) - i] = blend(leds[(kNumLeds - 1) - i], c, dim);
            }
        }
    }

    // ── I. 8-bar white strobe (always) ──────────────────────────────────────
    {
        uint8_t f = b.phraseView(32).sawTime(400);
        if (f > 0)
            for (auto& led : leds) led = blend(led, CRGB::White, f >> 1);
    }

    // ── J. 16-bar full burst (always) ───────────────────────────────────────
    {
        uint8_t f = b.phraseView(64).sawTime(600);
        if (f > 0)
            for (auto& led : leds) led = blend(led, CRGB::White, f);
    }
}

} // namespace Scarfnet
