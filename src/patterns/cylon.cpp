#include "patterns.h"

namespace Scarfnet {

// Two beams bounce end-to-end simultaneously with soft quadratic falloffs.
// Beam 1 (primary hue) and beam 2 (secondary hue) have independent speeds
// and drift cycles so they drift in and out of phase over time.
// Where they overlap, colors add (saturating) for a bright blend.
//
// Without beat: autonomous sweeps over their respective period ranges.
// With beat: each beam locks to a different beat multiple for independence.
//
// Drift works by shifting tMs ±kDriftAmpMs before the modulo. Since the drift
// rate is ≪ 1 ms/ms the effective time stays monotonically increasing — no jumps.
// Drift is suppressed in beat mode so the beams track their locked periods cleanly.
// Per-device localRnd phase offsets (up to 1/4 period) remain active in all modes.

void cylon(Leds& leds, const PatternContext& ctx)
{
    const uint32_t tMs = (uint32_t)ctx.timeMs;  // always-positive; safe past INT32_MAX

    // ── Shared constants ──────────────────────────────────────────────────────
    constexpr uint32_t kHueOscCycleMs     = 10000;  // period of the hue oscillation
    constexpr int8_t   kHueOscAmp         = 25;     // ±hue units around baseHue
    constexpr uint8_t  kSecColorMaxAmt    = 100;    // center accent blend at beam 1's core

    // ── Beam 1 (primary) ─────────────────────────────────────────────────────
    constexpr uint8_t  kBeam1WidthMin     = 4;      // narrowest beam 1 (pixels)
    constexpr uint8_t  kBeam1WidthMax     = 16;     // widest beam 1 (pixels)
    constexpr uint8_t  kBeam1BeatMult     = 2;      // beats per sweep when beat-active
    constexpr uint8_t  kBeam1PeriodMin    = 4;      // min autonomous period (seconds)
    constexpr uint8_t  kBeam1PeriodMax    = 30;     // max autonomous period (seconds)
    constexpr uint32_t kBeam1DriftCycleMs = 7000;   // drift oscillation period
    constexpr int32_t  kBeam1DriftAmpMs   = 1500;   // ±ms of time offset for beam 1

    // ── Beam 2 (secondary) ───────────────────────────────────────────────────
    constexpr uint8_t  kBeam2WidthMin     = 2;      // narrowest beam 2 (pixels)
    constexpr uint8_t  kBeam2WidthMax     = 8;      // widest beam 2 (pixels)
    constexpr uint8_t  kBeam2BeatMult     = 3;      // different multiple → independent motion
    constexpr uint8_t  kBeam2PeriodMin    = 10;     // min autonomous period (seconds)
    constexpr uint8_t  kBeam2PeriodMax    = 45;     // max autonomous period (seconds)
    constexpr uint32_t kBeam2DriftCycleMs = 53000;  // incommensurate with beam 1
    constexpr int32_t  kBeam2DriftAmpMs   = 300;    // subtler drift than beam 1

    // ── Per-device phase variation ────────────────────────────────────────────
    // localRnd shifts each beam's phase by up to 1/4 period so scarves in a
    // fleet are staggered rather than identical, but still move coherently.
    constexpr uint8_t kLocalPhaseSpread = 4;  // divisor: 1/4 period max offset

    // ── Hues ─────────────────────────────────────────────────────────────────
    const uint8_t baseHue      = (uint8_t)(tMs >> 10);
    const uint8_t secondaryHue = baseHue + rndRange(ctx.rnd >> 4, 64, 192);

    const uint16_t hueOscAngle = (uint16_t)((uint64_t)tMs * 65536u / kHueOscCycleMs);
    const int8_t   hueOsc      = (int8_t)((int32_t)sin16(hueOscAngle) * kHueOscAmp >> 15);
    const uint8_t  hue1        = baseHue + (uint8_t)hueOsc;
    const uint8_t  hue2        = secondaryHue;  // beam 2 runs in the secondary palette color

    // ── Beam 1 position ───────────────────────────────────────────────────────
    const uint32_t period1 = ctx.beat.isActive()
        ? (uint32_t)ctx.beat.intervalMs * kBeam1BeatMult
        : (uint32_t)rndRange(ctx.rnd, kBeam1PeriodMin, kBeam1PeriodMax) * 1000u;
    const int b1HalfWFP = max(1, rndRange(ctx.rnd, kBeam1WidthMin, kBeam1WidthMax) / 2) * 256;

    const uint16_t drift1Angle = (uint16_t)((uint64_t)tMs * 65536u / kBeam1DriftCycleMs);
    const int32_t  drift1Ms    = ctx.beat.isActive() ? 0
                               : ((int32_t)sin16(drift1Angle) * kBeam1DriftAmpMs) >> 15;
    const uint32_t local1Off   = (uint32_t)(uint8_t)ctx.localRnd * period1 / (256u * kLocalPhaseSpread);
    const uint8_t  frac1       = timeFrac8(tMs + (uint32_t)drift1Ms + local1Off, period1);
    const uint16_t center1FP   = (uint16_t)((uint32_t)quadwave8(frac1) * (uint32_t)((kNumLeds - 1) * 256u) / 255u);

    // ── Beam 2 position ───────────────────────────────────────────────────────
    const uint32_t period2 = ctx.beat.isActive()
        ? (uint32_t)ctx.beat.intervalMs * kBeam2BeatMult
        : (uint32_t)rndRange(ctx.rnd >> 6, kBeam2PeriodMin, kBeam2PeriodMax) * 1000u;
    const int b2HalfWFP = max(1, rndRange(ctx.rnd >> 6, kBeam2WidthMin, kBeam2WidthMax) / 2) * 256;

    const uint16_t drift2Angle = (uint16_t)((uint64_t)tMs * 65536u / kBeam2DriftCycleMs);
    const int32_t  drift2Ms    = ctx.beat.isActive() ? 0
                               : ((int32_t)sin16(drift2Angle) * kBeam2DriftAmpMs) >> 15;
    const uint32_t local2Off   = (uint32_t)(uint8_t)(ctx.localRnd >> 8) * period2 / (256u * kLocalPhaseSpread);
    const uint8_t  frac2       = timeFrac8(tMs + (uint32_t)drift2Ms + local2Off, period2);
    const uint16_t center2FP   = (uint16_t)((uint32_t)quadwave8(frac2) * (uint32_t)((kNumLeds - 1) * 256u) / 255u);

    // ── Render ────────────────────────────────────────────────────────────────
    // Beams are added (saturating) so overlapping regions brighten naturally.
    for (int i = 0; i < kNumLeds; ++i) {
        CRGB pixel = CRGB::Black;
        const int32_t iFP = (int32_t)((uint32_t)i * 256u);

        // Beam 1
        const int32_t dist1 = abs(iFP - (int32_t)center1FP);
        if (dist1 <= b1HalfWFP) {
            uint8_t edgeFrac = (uint8_t)((uint32_t)dist1 * 255u / (uint32_t)b1HalfWFP);
            uint8_t fade     = 255 - (uint8_t)(((uint16_t)edgeFrac * edgeFrac) >> 8);
            CRGB c = ColorFromPalette(ctx.palette, hue1, fade, LINEARBLEND);
            // Accent color peeks through at the beam core
            uint8_t secAmt = (uint8_t)((uint16_t)(255 - edgeFrac) * kSecColorMaxAmt >> 8);
            CRGB accent = ColorFromPalette(ctx.palette, secondaryHue, 255, LINEARBLEND);
            pixel += blend(c, accent, secAmt);
        }

        // Beam 2
        const int32_t dist2 = abs(iFP - (int32_t)center2FP);
        if (dist2 <= b2HalfWFP) {
            uint8_t edgeFrac = (uint8_t)((uint32_t)dist2 * 255u / (uint32_t)b2HalfWFP);
            uint8_t fade     = 255 - (uint8_t)(((uint16_t)edgeFrac * edgeFrac) >> 8);
            pixel += ColorFromPalette(ctx.palette, hue2, fade, LINEARBLEND);
        }

        leds[i] = pixel;
    }
}

} // namespace Scarfnet
