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

// ─── Beam falloff profile ─────────────────────────────────────────────────────

// The falloff is tested directly rather than inferred from rendered frames:
// cylon draws two beams with different widths, hues, and an accent-blended
// core, so whole-frame luma checks can't tell a falloff bug from the design.
//
// Guards the old uint8_t truncation overflow, which wrapped mid-beam distances
// to fade=255 — a bright spike inside an otherwise falling profile. Sweeps every
// half-width the pattern can produce (1..8 px) at full fixed-point resolution.
void test_cylon_beam_fade_full_at_center_zero_at_edge() {
    for (int32_t hw = 256; hw <= 8 * 256; hw += 256) {
        TEST_ASSERT_EQUAL_UINT8(255, cylonBeamFade(0, hw));
        TEST_ASSERT_EQUAL_UINT8(0, cylonBeamFade(hw, hw));
    }
}

void test_cylon_beam_fade_monotonic_from_center() {
    for (int32_t hw = 256; hw <= 8 * 256; hw += 256) {
        uint8_t prev = cylonBeamFade(0, hw);
        for (int32_t d = 1; d <= hw; ++d) {
            uint8_t fade = cylonBeamFade(d, hw);
            TEST_ASSERT_TRUE_MESSAGE(fade <= prev,
                "Beam fade increased moving away from center (falloff overflow)");
            prev = fade;
        }
    }
}

void test_cylon_beam_fade_is_quadratic() {
    // Halfway out, a quadratic falloff is at 3/4 brightness: 255 * 0.75 = 191.
    TEST_ASSERT_EQUAL_UINT8(191, cylonBeamFade(256, 512));
    TEST_ASSERT_EQUAL_UINT8(191, cylonBeamFade(1024, 2048));
}

// ---------------------------------------------------------------------------

void cylon_tests() {
    RUN_TEST(test_cylon_gradient_distinct_lumas_at_center_led);
    RUN_TEST(test_cylon_beam_fade_full_at_center_zero_at_edge);
    RUN_TEST(test_cylon_beam_fade_monotonic_from_center);
    RUN_TEST(test_cylon_beam_fade_is_quadratic);
}
