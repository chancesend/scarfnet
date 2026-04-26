#include "patterns.h"

namespace Scarfnet {

// Beat-synced sparse LED pattern.
//
// Each beat, 1–N LEDs snap on at deterministic positions and persist for
// holdBeats beats before expiring, creating a constant churn of additions
// and removals locked to the beat.
//
// Without tap-tempo: a synthetic ~30 BPM beat drives the same pixel logic,
// but phrase/wild events are suppressed — a slow, calm autonomous version.
//
// With tap-tempo, phrase events fire at 8/16/32-beat boundaries:
//   8-beat  (wildLevel 1) — a few extra pixels burst in, fast color spin
//   16-beat (wildLevel 2) — more pixels, more dramatic
//   32-beat (wildLevel 3) — dense burst + white strobe overlay

void digital(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
             const PatternContext& ctx)
{
    const BeatInfo& b     = ctx.beat;
    const Rnd       rnd   = ctx.rnd;
    const Rnd       local = ctx.localRnd;

    // When no tap-tempo is active, drive the same pixel logic with a synthetic
    // slow beat (~30 BPM = 2000 ms interval). Phrase/wild events are suppressed.
    constexpr uint16_t kSlowBeatMs = 2000;
    BeatInfo slowBeat {
        kSlowBeatMs,
        (uint16_t)((uint32_t)timeMs % kSlowBeatMs),
        (uint16_t)((uint32_t)timeMs / kSlowBeatMs)
    };
    const BeatInfo& active = b.isActive() ? b : slowBeat;

    const uint32_t beatNum = active.beatNumber;
    const uint8_t  baseHue = (uint8_t)(timeMs >> 7);  // ~8 s full cycle

    // Session-fixed parameters
    const uint8_t holdBeats  = rndRange(rnd,      2, 5);  // beats each pixel persists
    const uint8_t addPerBeat = rndRange(rnd >> 4, 2, 7);  // base LEDs added per beat

    // Wild level: only active with tap-tempo; stacks at 8/16/32-beat boundaries
    uint8_t wildLevel = 0;
    if (b.isActive()) {
        if (beatNum % 8  == 0) wildLevel = 1;
        if (beatNum % 16 == 0) wildLevel = 2;
        if (beatNum % 32 == 0) wildLevel = 3;
    }

    // Per-phrase character: shifts the wild accent hue each 8-beat phrase
    const uint8_t phraseChar = (uint8_t)((beatNum / 8u) * 97u ^ (uint8_t)rnd);

    // ── Build lit set from sliding window of recent beats ───────────────────
    // Iterate newest→oldest; first match wins so newer additions take priority.
    // litAge: 0=dark, 1=oldest/dimmest, holdBeats=just added/brightest.
    uint8_t litAge[kNumLeds] = {};

    for (uint8_t k = 0; k < holdBeats; ++k) {
        uint32_t pastBeat = beatNum - (uint32_t)k;
        // Current beat gets the wild boost; past beats run at base rate
        uint8_t kAdd   = (k == 0) ? addPerBeat + wildLevel * 2 : addPerBeat;
        uint8_t numAdd = rndRange((Rnd)((uint16_t)(pastBeat * 137u) ^ rnd), 1, kAdd + 1);
        for (uint8_t j = 0; j < numAdd; ++j) {
            uint8_t pos = (uint8_t)((pastBeat * (uint32_t)(j * 61u + 23u) ^ (uint32_t)local) % kNumLeds);
            if (litAge[pos] == 0) litAge[pos] = holdBeats - k;
        }
    }

    // ── Render lit pixels ───────────────────────────────────────────────────
    for (int i = 0; i < kNumLeds; ++i) {
        if (litAge[i] == 0) { leds[i] = CRGB::Black; continue; }

        // ageRatio: 255=newest/brightest, low=oldest/dimmest
        constexpr uint8_t kMinBrightness = 80;   // oldest pixels (about to expire)
        constexpr uint8_t kMaxBrightness = 200;  // freshly lit pixels
        uint8_t ageRatio = (uint8_t)((uint16_t)litAge[i] * 255u / holdBeats);
        uint8_t bright   = lerp8by8(kMinBrightness, kMaxBrightness, ageRatio);

        // Per-LED stable hue from localRnd so each scarf has its own color layout
        uint8_t hue = baseHue + (uint8_t)((uint32_t)i * 13u + (uint8_t)(local >> 8));
        leds[i] = ColorFromPalette(palette, hue, bright, LINEARBLEND);
    }

    // ── Wild phrase overlay (tap-tempo only) ────────────────────────────────
    if (wildLevel > 0) {
        // Decay window scales with wildLevel: longer decay = more dramatic
        uint8_t wildFlash = active.sawTime((uint16_t)(active.intervalMs * wildLevel));
        if (wildFlash > 0) {
            // Fast color spin + phraseChar shifts starting hue each phrase
            uint8_t wildHue   = phraseChar + (uint8_t)(timeMs >> 3);
            uint8_t wildCount = wildLevel * 3;
            for (uint8_t j = 0; j < wildCount; ++j) {
                uint8_t pos = (uint8_t)((beatNum * (uint32_t)(j * 53u + 7u) ^ (uint32_t)local) % kNumLeds);
                CRGB c = ColorFromPalette(palette, wildHue + (uint8_t)(j * 30u), wildFlash, LINEARBLEND);
                leds[pos] = blend(leds[pos], c, wildFlash);
            }
            // 32-beat boundary: white strobe on top
            if (wildLevel == 3)
                for (auto& led : leds) led = blend(led, CRGB::White, wildFlash >> 2);
        }
    }
}

} // namespace Scarfnet
