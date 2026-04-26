#include "patterns.h"

namespace Scarfnet {

void colorwaves(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, const BeatInfo& beat)
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
    // Incommensurate values keep the pattern from repeating on short cycles.
    constexpr uint16_t kBrightDepthBpm88  = 341;   // ~1.33 BPM brightness depth oscillation
    constexpr uint16_t kBrightThetaBpm88  = 203;   // ~0.79 BPM brightness wave spacing
    constexpr uint16_t kHueIncBpm88       = 113;   // ~0.44 BPM hue scroll speed oscillation

    // Brightness depth range [96, 224]: higher min = less contrast; higher max = deeper pulses
    constexpr uint8_t  kBrightDepthMin    = 96;
    constexpr uint8_t  kBrightDepthMax    = 224;
    // Brightness wave spacing range (in 8.8 units): controls LED-to-LED phase offset
    constexpr uint16_t kBrightThetaMin    = 25 * 256;
    constexpr uint16_t kBrightThetaMax    = 40 * 256;
    // Hue scroll increment range [300, 1500]: wider range = more dramatic hue sweep
    constexpr uint16_t kHueIncMin         = 300;
    constexpr uint16_t kHueIncMax         = 1500;
    // How many times faster hue scrolls on the last beat of a phrase
    constexpr uint8_t  kPhraseHueMult     = 5;
    // How quickly new colors blend in each frame [0=no update, 255=instant]
    constexpr uint8_t  kBlendAmount       = 128;

    uint8_t  brightdepth        = (uint8_t)beatsinT(kBrightDepthBpm88, kBrightDepthMin, kBrightDepthMax);
    uint16_t brightnessthetainc = beatsinT(kBrightThetaBpm88, kBrightThetaMin, kBrightThetaMax);
    uint16_t hueinc16           = beatsinT(kHueIncBpm88, kHueIncMin, kHueIncMax);

    // On the last beat of every 16-beat phrase, run the hue 5× faster to build
    // tension before the phrase boundary.
    if (beat.isActive() && beat.isLastBeatOfPhrase(16))
        hueinc16 *= kPhraseHueMult;

    // Pseudotime: time-integral of msmultiplier (beatsin88(147, 23, 60), avg ≈ 41/ms).
    // When beat is active, compute the exact integral: 15 beats at 1× rate then 1 beat
    // at 5× rate per phrase — no discontinuity at boundaries.
    uint16_t pseudotime;
    if (beat.isActive() && beat.intervalMs > 0) {
        uint32_t iMs         = beat.intervalMs;
        uint32_t phraseMs    = iMs * 16;
        uint32_t t           = (uint32_t)timeMs;
        uint32_t nPhrases    = t / phraseMs;
        uint32_t inPhrase    = t % phraseMs;
        uint32_t lastBeatMs  = iMs * 15;
        // Per-phrase contribution: 15 normal beats + 1 fast beat (5×)
        uint32_t phraseContrib = lastBeatMs * 41u + iMs * 205u;
        uint32_t pt = nPhrases * phraseContrib;
        pt += (inPhrase <= lastBeatMs) ? inPhrase * 41u
                                       : lastBeatMs * 41u + (inPhrase - lastBeatMs) * 205u;
        pseudotime = (uint16_t)pt;
    } else {
        pseudotime = (uint16_t)((uint32_t)timeMs * 41u);
    }

    // sHue16: time-integral of hue scroll rate (beatsin88(400, 5, 9), avg ≈ 7/ms).
    uint16_t hue16 = (uint16_t)((uint32_t)timeMs * 7u);

    for (uint16_t i = 0; i < leds.size(); i++) {
        hue16 += hueinc16;
        uint16_t h16_128 = hue16 >> 7;
        uint8_t hue8 = (h16_128 & 0x100) ? 255 - (h16_128 >> 1) : h16_128 >> 1;

        uint16_t brightnesstheta = pseudotime + (uint16_t)((uint32_t)(i + 1) * brightnessthetainc);
        uint16_t b16  = sin16(brightnesstheta) + 32768;
        uint16_t bri16 = (uint32_t)b16 * b16 / 65536;
        uint8_t  bri8  = (uint32_t)bri16 * brightdepth / 65536;
        bri8 += (255 - brightdepth);

        uint8_t index = scale8(hue8, 240);
        CRGB newcolor = ColorFromPalette(palette, index, bri8);

        uint16_t pixelnumber = (leds.size() - 1) - i;
        nblend(leds[pixelnumber], newcolor, kBlendAmount);
    }
}

} // namespace Scarfnet
