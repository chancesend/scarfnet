#include "patterns.h"

namespace Scarfnet {

void pride(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat, Rnd rnd, Rnd localRnd)
{
    // All animation parameters derived from timeMs — fully stateless.
    // beatsinT replaces FastLED's beatsin88 (which uses millis()) with a timeMs-based
    // equivalent so all scarves stay phase-locked on the same mesh time.
    // bpm88 is in FastLED's 8.8 fixed-point format (same as beatsin88) — divide by 256 for actual BPM.
    auto beatsinT = [timeMs](uint16_t bpm88, uint16_t lo, uint16_t hi) -> uint16_t {
        uint16_t phase = (uint16_t)((uint64_t)(uint32_t)timeMs * bpm88 * 256 / 60000);
        return lo + (uint16_t)((uint32_t)(hi - lo) * (uint16_t)((int32_t)sin16(phase) + 32768) / 65536);
    };

    // Oscillator rates (8.8 fixed-point BPM: divide by 256 for actual BPM).
    constexpr uint16_t kBrightDepthBpm88  = 341;   // ~1.33 BPM brightness depth oscillation
    constexpr uint16_t kBrightThetaBpm88  = 203;   // ~0.79 BPM brightness wave spacing
    constexpr uint16_t kHueIncBpm88       = 113;   // ~0.44 BPM hue scroll speed oscillation

    // Brightness depth range [96, 224]: higher min = less contrast; higher max = deeper pulses
    constexpr uint8_t  kBrightDepthMin    = 96;
    constexpr uint8_t  kBrightDepthMax    = 224;
    // Brightness wave spacing range (in 8.8 units)
    constexpr uint16_t kBrightThetaMin    = 25 * 256;
    constexpr uint16_t kBrightThetaMax    = 40 * 256;
    // Hue scroll increment range: [1, 3000] → very wide sweep for rainbow-y feel
    constexpr uint16_t kHueIncMin         = 1;
    constexpr uint16_t kHueIncMax         = 3000;
    // How quickly new colors blend in each frame [0=no update, 255=instant]
    constexpr uint8_t  kBlendAmount       = 64;

    uint8_t  brightdepth        = (uint8_t)beatsinT(kBrightDepthBpm88, kBrightDepthMin, kBrightDepthMax);
    uint16_t brightnessthetainc = beatsinT(kBrightThetaBpm88, kBrightThetaMin, kBrightThetaMax);
    uint16_t hueinc16           = beatsinT(kHueIncBpm88, kHueIncMin, kHueIncMax);

    // Per-device variation from localRnd — subtle enough to read as a cloud, not independent.
    // localPhase:   fixed offset into the brightness wave so peaks land at different positions.
    // localHueBias: small palette shift [0..~20 hue8 steps] for color tinting.
    // localRate:    per-device pseudotime advance speed slightly above/below the fleet average,
    //               so brightness peaks drift apart over minutes without ever looking detached.
    const uint8_t localPhase   = (uint8_t)localRnd;
    const uint8_t localHueBias = (uint8_t)(localRnd >> 8);
    constexpr uint8_t  kPseudoRateBase    = 38;   // min per-device pseudotime advance rate (ms⁻¹)
    constexpr uint8_t  kPseudoRateSpread  = 7;    // range above base (38–44 vs fleet average 41)
    constexpr uint8_t  kHueBiasScale      = 20;   // hue8 units of per-device palette offset [0..20]

    // Pseudotime: time-integral of msmultiplier (beatsin88(147, 23, 60), avg ≈ 41/ms).
    // Each device advances at its own rate (kPseudoRateBase..kPseudoRateBase+kPseudoRateSpread)
    // plus a fixed phase offset — brightness peaks start displaced and drift apart over time.
    const uint8_t localPseudoRate = kPseudoRateBase + (uint8_t)(localPhase % kPseudoRateSpread);
    uint16_t pseudotime = (uint16_t)((uint32_t)timeMs * localPseudoRate)
                        + (uint16_t)((uint32_t)localPhase << 8);  // fixed phase displacement

    // sHue16: time-integral of hue scroll rate (beatsin88(400, 5, 9), avg ≈ 7/ms).
    // Per-device hue bias shifts each scarf into a slightly different palette neighborhood.
    uint16_t hue16 = (uint16_t)((uint32_t)timeMs * 7u)
                   + (uint16_t)((uint32_t)localHueBias * kHueBiasScale);

    // ── Phrase events ────────────────────────────────────────────────────────
    // At phrase boundaries there's a session-fixed random chance of a
    // burst. Each burst is an additive phase offset that peaks at the boundary
    // and decays over one beat — looks like the pattern suddenly jumps forward.
    //
    //   Brightness burst  (pseudotime phase shift)
    const uint16_t kBrightnessBurstBeatEvent = 8;
    //   Color burst       (hue16 phase shift)
    const uint16_t kColorBurstBeatEvent = 8;
    //   Both at once      (most dramatic)
    const uint16_t kEverythingBurstBeatEvent = 32;
    //
    // All scarves respond identically since rnd is fleet-synced.
    if (beat.isActive() && beat.intervalMs > 0) {
        const uint32_t beatNum = beat.beatNumber;

        // Brightness speedup — phase-jumps the brightness oscillation
        {
            uint8_t thresh = rndRange(rnd, 80, 200);  // ~31–78% chance per phrase
            bool triggered = ((uint8_t)((beatNum / kBrightnessBurstBeatEvent) * 127u ^ (uint8_t)rnd)) < thresh;
            if (triggered) {
                uint8_t burst = beat.phraseView(kBrightnessBurstBeatEvent).sawTime(beat.intervalMs);
                pseudotime += (uint16_t)((uint32_t)burst * 96u);
            }
        }

        // Color burst — rapidly shifts the global hue position
        {
            uint8_t thresh = rndRange(rnd >> 4, 80, 200);
            bool triggered = ((uint8_t)((beatNum / kColorBurstBeatEvent) * 211u ^ (uint8_t)(rnd >> 4))) < thresh;
            if (triggered) {
                uint8_t burst = beat.phraseView(kColorBurstBeatEvent).sawTime(beat.intervalMs);
                hue16 += (uint16_t)((uint32_t)burst * 160u);
            }
        }

        // Both effects simultaneously — most dramatic phrase moment
        {
            uint8_t thresh = rndRange(rnd >> 8, 100, 220);  // slightly higher floor
            bool triggered = ((uint8_t)((beatNum / kEverythingBurstBeatEvent) * 179u ^ (uint8_t)(rnd >> 8))) < thresh;
            if (triggered) {
                uint8_t burst = beat.phraseView(kEverythingBurstBeatEvent).sawTime(beat.intervalMs);
                pseudotime += (uint16_t)((uint32_t)burst * 128u);
                hue16      += (uint16_t)((uint32_t)burst * 192u);
            }
        }
    }

    for (uint16_t i = 0; i < kNumLeds; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        uint16_t brightnesstheta = pseudotime + (uint16_t)((uint32_t)(i + 1) * brightnessthetainc);
        uint16_t b16  = sin16(brightnesstheta) + 32768;
        uint16_t bri16 = (uint32_t)b16 * b16 / 65536;
        uint8_t  bri8  = (uint32_t)bri16 * brightdepth / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = ColorFromPalette(palette, hue8, bri8, LINEARBLEND);
        nblend(leds[(kNumLeds - 1) - i], newcolor, kBlendAmount);
    }

    // On-beat palette-color flash on ~50% of pixels.
    // Color is fleet-synced (from rnd); pixel selection is per-device and
    // changes each beat (localRnd × beatNumber hash).
    if (beat.isActive()) {
        uint8_t flash = beat.sawTime();
        if (flash > 0) {
            CRGB flashColor = ColorFromPalette(palette, (uint8_t)(rnd * 17u >> 8), 255, LINEARBLEND);
            uint32_t beatSeed = beat.beatNumber * 137u ^ (uint32_t)localRnd;
            for (int i = 0; i < kNumLeds; ++i) {
                if (((uint8_t)((uint32_t)i * 73u ^ beatSeed)) < 128)
                    leds[i] = blend(leds[i], flashColor, flash / 2);
            }
        }
    }
}

} // namespace Scarfnet
