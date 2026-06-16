#include <unity.h>

#include "../../../src/patterns/cylon.cpp"

using namespace Scarfnet;

static Leds makeLeds() { return Leds(kNumLeds, CRGB::Black); }

// rnd=0, localRnd=0, no beat → fully deterministic (period1=4000ms, b1HalfWFP=2px)
static PatternContext makeCylonCtx(int32_t tMs) {
    PatternContext ctx = {};
    ctx.timeMs   = tMs;
    ctx.palette  = RainbowColors_p;
    ctx.rnd      = 0;
    ctx.localRnd = 0;
    return ctx;
}

// ─── Quantization: distinct luma values ───────────────────────────────────────

// Track all distinct luma values seen at LED 12 (mid-strip) as both beams
// sweep over one full period (4000 ms with rnd=0).  With sub-pixel center
// positions, the beam produces a smooth brightness gradient — many distinct
// luma levels as it approaches and recedes.  An integer-position beam of
// half-width 2 px would produce at most 3 distinct luma levels; sub-pixel
// motion with a smooth quadratic falloff should yield far more.
void test_cylon_gradient_distinct_lumas_at_center_led() {
    bool seen[256] = {};

    for (int32_t t = 0; t < 4000; t += 2) {
        Leds leds = makeLeds();
        cylon(leds, makeCylonCtx(t));
        uint8_t luma = leds[12].getLuma();
        if (luma > 5) seen[luma] = true;
    }

    int count = 0;
    for (int i = 0; i < 256; ++i) if (seen[i]) ++count;

    TEST_ASSERT_TRUE_MESSAGE(count > 30,
        "Too few distinct luma values at LED 12 — beam falloff may be quantized");
}

// ─── No brightness spikes ─────────────────────────────────────────────────────

// For every frame in a full period sweep, no interior LED should have
// significantly higher luma than both its neighbours.  Such a spike is the
// specific artifact produced by the uint8_t truncation overflow in the old
// quadratic falloff: (uint8_t)(edgeFrac16² >> 16) wraps for mid-beam
// distances (e.g. edgeFrac16=32768 → result 16384 → uint8_t=0 → fade=255).
void test_cylon_no_brightness_spikes() {
    for (int32_t t = 0; t < 4000; t += 2) {
        Leds leds = makeLeds();
        cylon(leds, makeCylonCtx(t));

        for (int i = 1; i < kNumLeds - 1; ++i) {
            int diffPrev = (int)leds[i].getLuma() - (int)leds[i-1].getLuma();
            int diffNext = (int)leds[i].getLuma() - (int)leds[i+1].getLuma();
            bool spike   = leds[i].getLuma() > 50 && diffPrev > 40 && diffNext > 40;
            TEST_ASSERT_FALSE_MESSAGE(spike,
                "Brightness spike: interior LED much brighter than both neighbours");
        }
    }
}

// ─── Monotonicity from peak ───────────────────────────────────────────────────

// When exactly one beam peak is visible in the interior of the strip, luma
// must be non-increasing in both directions from that peak — a correct
// quadratic beam profile has no reversals within its span.  Frames where
// both beams overlap (two interior peaks) are skipped so the check is never
// confused by additive blending of two independent beams.
void test_cylon_beam_monotonic_from_peak() {
    int framesChecked = 0;

    for (int32_t t = 0; t < 4000; t += 2) {
        Leds leds = makeLeds();
        cylon(leds, makeCylonCtx(t));

        // Find interior local maxima: brighter than both neighbours, above threshold
        int peakPos = -1, peakCount = 0;
        for (int i = 2; i < kNumLeds - 2; ++i) {
            uint8_t l = leds[i].getLuma();
            if (l > 30 && l > leds[i-1].getLuma() && l > leds[i+1].getLuma()) {
                peakPos = i;
                peakCount++;
            }
        }
        if (peakCount != 1) continue;  // skip overlap frames and edge-beam frames
        framesChecked++;

        // Check 3 LED steps outward on each side.  With rnd=0, b1HalfWFP=2px
        // so 3 steps covers the full beam span including the fade-to-zero edge.
        for (int s = 0; s < 3 && peakPos + s < kNumLeds - 1; ++s) {
            TEST_ASSERT_TRUE_MESSAGE(
                leds[peakPos + s].getLuma() >= leds[peakPos + s + 1].getLuma(),
                "Luma increased moving right from beam peak (non-monotonic falloff)");
        }
        for (int s = 0; s < 3 && peakPos - s > 0; ++s) {
            TEST_ASSERT_TRUE_MESSAGE(
                leds[peakPos - s].getLuma() >= leds[peakPos - s - 1].getLuma(),
                "Luma increased moving left from beam peak (non-monotonic falloff)");
        }
    }

    TEST_ASSERT_TRUE_MESSAGE(framesChecked > 0,
        "No single-peak frames found — increase sweep range or lower threshold");
}

// ---------------------------------------------------------------------------

void cylon_tests() {
    RUN_TEST(test_cylon_gradient_distinct_lumas_at_center_led);
    RUN_TEST(test_cylon_no_brightness_spikes);
    RUN_TEST(test_cylon_beam_monotonic_from_peak);
}
