#include <unity.h>
#include <builtin_led.h>

using Scarfnet::FlashBurst;
using Scarfnet::IndicatorState;
using Scarfnet::builtinLedColor;

// ─── FlashBurst ──────────────────────────────────────────────────────────────

void test_flash_burst_inactive_until_triggered()
{
    FlashBurst b(3, 100);
    TEST_ASSERT_FALSE(b.isActive(1000));
    TEST_ASSERT_FALSE(b.isOn(1000));
}

void test_flash_burst_alternates_on_off()
{
    FlashBurst b(3, 100);
    b.trigger(1000);
    TEST_ASSERT_TRUE(b.isOn(1000));
    TEST_ASSERT_TRUE(b.isOn(1099));
    TEST_ASSERT_FALSE(b.isOn(1100));
    TEST_ASSERT_TRUE(b.isOn(1200));
    TEST_ASSERT_TRUE(b.isActive(1150));  // active through the "off" halves too
}

void test_flash_burst_ends_after_count_flashes()
{
    FlashBurst b(3, 100);
    b.trigger(1000);
    TEST_ASSERT_TRUE(b.isActive(1599));
    TEST_ASSERT_FALSE(b.isActive(1600));
    TEST_ASSERT_FALSE(b.isOn(1000 + 0xFFFFFFFFu));  // stays off; no wraparound re-fire
}

void test_flash_burst_retrigger_restarts()
{
    FlashBurst b(3, 100);
    b.trigger(1000);
    b.trigger(1500);
    TEST_ASSERT_TRUE(b.isActive(2000));
}

// ─── builtinLedColor priorities ──────────────────────────────────────────────

static IndicatorState idle()
{
    IndicatorState s = {};
    s.meshTimeMs = kSyncBlinkPeriodMs / 2;  // outside the sync blink window
    return s;
}

void test_led_off_when_idle()
{
    TEST_ASSERT_TRUE(builtinLedColor(idle()) == CRGB::Black);
}

void test_led_sync_blink_blue_then_off()
{
    IndicatorState s = idle();
    s.meshTimeMs = kSyncBlinkPeriodMs * 7;  // start of a period
    TEST_ASSERT_TRUE(builtinLedColor(s) == CRGB::Blue);
    s.meshTimeMs = kSyncBlinkPeriodMs * 7 + kSyncBlinkPeriodMs / 20;  // 5% in
    TEST_ASSERT_TRUE(builtinLedColor(s) == CRGB::Black);
}

void test_led_sync_blink_red_when_newer_firmware_seen()
{
    IndicatorState s = idle();
    s.meshTimeMs = 0;
    s.newerVersionSeen = true;
    TEST_ASSERT_TRUE(builtinLedColor(s) == CRGB::Red);
}

void test_led_tap_tempo_dim_glow_and_beat_pulse()
{
    IndicatorState s = idle();
    s.tapTempoMode = true;
    TEST_ASSERT_TRUE(builtinLedColor(s) == CRGB(0, 5, 0));

    s.tempoActive    = true;
    s.beatIntervalMs = 500;
    s.beatPhaseMs    = 10;   // within first 10% of the beat
    TEST_ASSERT_TRUE(builtinLedColor(s) == CRGB::Green);
    s.beatPhaseMs    = 60;
    TEST_ASSERT_TRUE(builtinLedColor(s) == CRGB(0, 5, 0));
}

void test_led_tap_tempo_overrides_sync_blink()
{
    IndicatorState s = idle();
    s.meshTimeMs   = 0;  // inside sync blink window
    s.tapTempoMode = true;
    TEST_ASSERT_TRUE(builtinLedColor(s) == CRGB(0, 5, 0));
}

void test_led_alarm_overrides_everything()
{
    IndicatorState s = idle();
    s.meshTimeMs     = 0;
    s.tapTempoMode   = true;
    s.tempoActive    = true;
    s.beatIntervalMs = 500;
    s.alarmActive    = true;
    s.alarmOn        = true;
    TEST_ASSERT_TRUE(builtinLedColor(s) == CRGB::Red);
    s.alarmOn = false;  // "off" half of a flash is dark, not the underlying mode
    TEST_ASSERT_TRUE(builtinLedColor(s) == CRGB::Black);
}

void builtin_led_tests()
{
    RUN_TEST(test_flash_burst_inactive_until_triggered);
    RUN_TEST(test_flash_burst_alternates_on_off);
    RUN_TEST(test_flash_burst_ends_after_count_flashes);
    RUN_TEST(test_flash_burst_retrigger_restarts);
    RUN_TEST(test_led_off_when_idle);
    RUN_TEST(test_led_sync_blink_blue_then_off);
    RUN_TEST(test_led_sync_blink_red_when_newer_firmware_seen);
    RUN_TEST(test_led_tap_tempo_dim_glow_and_beat_pulse);
    RUN_TEST(test_led_tap_tempo_overrides_sync_blink);
    RUN_TEST(test_led_alarm_overrides_everything);
}
