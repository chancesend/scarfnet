#include <unity.h>

// Include the debug pattern source directly so static helpers (nodeColor) are
// visible in this translation unit without changing their linkage.
#include "../../../src/patterns/debug.cpp"

using namespace Scarfnet;

static Leds makeLeds(int n = 25) { return Leds(n, CRGB::Black); }

static PatternContext makeCtx() {
    PatternContext ctx = {};
    ctx.timeMs      = 10000;
    ctx.nodeId      = 0x12345678u;
    ctx.lastPressId = 0u;
    ctx.flashCount  = 0;
    return ctx;
}

// ─── nodeColor ────────────────────────────────────────────────────────────────

void test_debug_node_color_deterministic() {
    // Same ID must always map to the same RGB.
    CRGB a = nodeColor(0xDEADBEEFu);
    CRGB b = nodeColor(0xDEADBEEFu);
    TEST_ASSERT_EQUAL_UINT8(a.r, b.r);
    TEST_ASSERT_EQUAL_UINT8(a.g, b.g);
    TEST_ASSERT_EQUAL_UINT8(a.b, b.b);
}

void test_debug_node_color_distinct_ids() {
    // Spot-check: two IDs that differ by one bit produce different colors.
    CRGB a = nodeColor(0x00000001u);
    CRGB b = nodeColor(0x00000002u);
    TEST_ASSERT_TRUE(a.r != b.r || a.g != b.g || a.b != b.b);
}

void test_debug_node_color_nonzero() {
    // With saturation=220 and value=255, every ID maps to a non-black color.
    for (uint32_t id : {0u, 1u, 42u, 0xFFFFFFFFu})
        TEST_ASSERT_TRUE(nodeColor(id).r > 0 || nodeColor(id).g > 0 || nodeColor(id).b > 0);
}

// ─── debug(): base glow ───────────────────────────────────────────────────────

void test_debug_base_glow_nonzero() {
    Leds leds = makeLeds();
    debug(leds, makeCtx());
    bool anyNonBlack = false;
    for (auto& led : leds) anyNonBlack |= (led.r || led.g || led.b);
    TEST_ASSERT_TRUE(anyNonBlack);
}

// ─── debug(): flash overlay ───────────────────────────────────────────────────

void test_debug_fresh_flash_adds_brightness() {
    Leds leds_no_flash = makeLeds();
    Leds leds_flash    = makeLeds();
    PatternContext ctx = makeCtx();

    debug(leds_no_flash, ctx);

    ctx.flashCount = 1;
    ctx.recentFlashes[0] = {0xAABBCCDDu, ctx.timeMs};  // elapsed = 0 → full flash
    debug(leds_flash, ctx);

    bool brighter = false;
    for (size_t i = 0; i < leds_flash.size(); i++) {
        if (leds_flash[i].r > leds_no_flash[i].r ||
            leds_flash[i].g > leds_no_flash[i].g ||
            leds_flash[i].b > leds_no_flash[i].b) { brighter = true; break; }
    }
    TEST_ASSERT_TRUE(brighter);
}

void test_debug_expired_flash_has_no_effect() {
    Leds leds_no_flash = makeLeds();
    Leds leds_expired  = makeLeds();
    PatternContext ctx = makeCtx();

    debug(leds_no_flash, ctx);

    ctx.flashCount = 1;
    ctx.recentFlashes[0] = {0xAABBCCDDu, 0};  // elapsed = 10000ms >> kFlashFadeMs
    debug(leds_expired, ctx);

    for (size_t i = 0; i < leds_no_flash.size(); i++) {
        TEST_ASSERT_EQUAL_UINT8(leds_no_flash[i].r, leds_expired[i].r);
        TEST_ASSERT_EQUAL_UINT8(leds_no_flash[i].g, leds_expired[i].g);
        TEST_ASSERT_EQUAL_UINT8(leds_no_flash[i].b, leds_expired[i].b);
    }
}

// ─── debug(): sparkle ─────────────────────────────────────────────────────────

void test_debug_no_white_sparkle_when_not_last_press() {
    // With nodeId != lastPressId, no pixel should ever reach pure white (255,255,255).
    PatternContext ctx = makeCtx();
    ctx.lastPressId = 0x99999999u;  // different from nodeId

    for (int frame = 0; frame < 200; frame++) {
        Leds leds = makeLeds();
        ctx.timeMs += 15;
        debug(leds, ctx);
        for (auto& led : leds)
            TEST_ASSERT_TRUE(!(led.r == 255 && led.g == 255 && led.b == 255));
    }
}

// ---------------------------------------------------------------------------

void patterns_tests() {
    RUN_TEST(test_debug_node_color_deterministic);
    RUN_TEST(test_debug_node_color_distinct_ids);
    RUN_TEST(test_debug_node_color_nonzero);
    RUN_TEST(test_debug_base_glow_nonzero);
    RUN_TEST(test_debug_fresh_flash_adds_brightness);
    RUN_TEST(test_debug_expired_flash_has_no_effect);
    RUN_TEST(test_debug_no_white_sparkle_when_not_last_press);
}
