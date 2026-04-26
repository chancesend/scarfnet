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

    uint8_t  brightdepth        = (uint8_t)beatsinT(341, 96, 224);
    uint16_t brightnessthetainc = beatsinT(203, 25 * 256, 40 * 256);
    uint16_t hueinc16           = beatsinT(113, 1, 3000);

    // Pseudotime: time-integral of msmultiplier (beatsin88(147, 23, 60), avg ≈ 41/ms).
    // Linear approximation at the average rate — wraps naturally as uint16_t.
    uint16_t pseudotime = (uint16_t)((uint32_t)timeMs * 41u);

    // sHue16: time-integral of hue scroll rate (beatsin88(400, 5, 9), avg ≈ 7/ms).
    uint16_t hue16 = (uint16_t)((uint32_t)timeMs * 7u);

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
        nblend(leds[(kNumLeds - 1) - i], newcolor, 64);
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
