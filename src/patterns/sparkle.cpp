#include "patterns.h"

namespace Scarfnet {

// Dim palette-tinted base with occasional colored sparks and rare white flashes.
//
// This pattern carries state in the leds array between frames — the slow
// fadeToBlack each tick creates soft decay trails without static variables.
//
// Spark rate is normally very low (~1-6); an organic noise burst boosts it
// roughly 1-2 times per 20-minute session. Beat onset adds a moderate pop.
//
// At the last 2 beats of every 16/32/64-beat phrase the strip erupts in a
// dense white sparkle burst, but only during the ¼-beat sub-slot assigned to
// this device by localRnd — so the fleet ripples rather than all firing at once.
void sparkle(Leds& leds, const PatternContext& ctx)
{
    constexpr uint8_t  kFadeAmount         = 30;    // per-frame decay; higher = shorter trails
    constexpr uint32_t kHueDriftDivisor    = 150;   // larger = slower hue drift through palette
    constexpr uint8_t  kBaseBrightness     = 35;    // dim glow between sparks
    constexpr uint8_t  kBaseBlendAmt       = 18;    // how strongly base color blends each frame
    constexpr uint8_t  kWhiteChance        = 42;    // out of 255; ~1-in-6 sparks are white
    constexpr uint8_t  kHueSpread          = 48;    // hue variation range around base hue
    constexpr uint8_t  kSparkBrightness    = 220;   // brightness of palette-colored sparks
    constexpr uint8_t  kRateBase           = 1;     // baseline sparks per frame (very sparse)
    constexpr uint8_t  kRateMax            = 6;     // max baseline rate before burst/beat boosts
    constexpr uint32_t kBurstNoiseTimeDiv  = 18000; // noise x-axis: ~18s per unit → slow clusters
    constexpr uint8_t  kBurstNoiseThresh   = 195;   // noise values above this trigger a burst
    constexpr uint8_t  kBurstRateScale     = 3;     // burst excess × this = extra spark rate
    constexpr uint8_t  kBurstRateCap       = 70;    // max extra rate from burst noise
    constexpr uint8_t  kBeatBump           = 60;    // extra rate on each beat onset
    constexpr uint16_t kBeatWindowMs       = 80;    // onset window for beat bump

    // ── Spark rate ────────────────────────────────────────────────────────────
    uint8_t sparkleRate = rndRange(ctx.rnd, kRateBase, kRateMax);

    // Organic burst: rare, slow noise cluster (~1-2 bursts per 20-min session)
    uint8_t burstNoise = inoise8((uint32_t)ctx.timeMs / kBurstNoiseTimeDiv, (uint8_t)ctx.rnd);
    if (burstNoise > kBurstNoiseThresh) {
        uint8_t excess = burstNoise - kBurstNoiseThresh;
        sparkleRate = qadd8(sparkleRate, (uint8_t)min((int)excess * kBurstRateScale, (int)kBurstRateCap));
    }

    // Beat bump — moderate pop, not a full whiteout
    if (ctx.beat.isOnBeat(kBeatWindowMs)) sparkleRate = qadd8(sparkleRate, kBeatBump);

    // ── Base render ───────────────────────────────────────────────────────────
    fadeToBlackBy(leds.data(), kNumLeds, kFadeAmount);

    uint8_t hue = (uint8_t)((uint32_t)ctx.timeMs / kHueDriftDivisor);
    CRGB baseColor = ColorFromPalette(ctx.palette, hue, kBaseBrightness, LINEARBLEND);
    for (auto& led : leds) led = blend(led, baseColor, kBaseBlendAmt);

    // Mostly palette-colored sparks; occasional white for a natural twinkle mix
    for (int i = 0; i < kNumLeds; ++i) {
        if (random8() < sparkleRate) {
            if (random8() < kWhiteChance) {
                leds[i] = CRGB::White;
            } else {
                uint8_t sparkHue = hue + random8(kHueSpread);
                leds[i] = ColorFromPalette(ctx.palette, sparkHue, kSparkBrightness, LINEARBLEND);
            }
        }
    }

    // ── Wild-zone cascade ─────────────────────────────────────────────────────
    // Dense white sparkle burst during this device's assigned sub-slot at phrase
    // endings. Density is proportional to wildBright so the burst has texture.
    uint8_t wild = wildZoneBright(ctx.beat, ctx.localRnd);
    if (wild > 0) {
        for (int i = 0; i < kNumLeds; ++i) {
            if (random8() < wild) leds[i] = CRGB::White;
        }
    }
}

} // namespace Scarfnet
