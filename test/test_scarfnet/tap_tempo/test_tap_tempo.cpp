#include <unity.h>
#include <tap_tempo.h>

using Scarfnet::TapTempo;

// ─── Initial state ───────────────────────────────────────────────────────────

void test_tap_tempo_initial_not_active() {
    TapTempo tt;
    TEST_ASSERT_FALSE(tt.isActive());
    TEST_ASSERT_EQUAL_UINT16(0, tt.beatIntervalMs());
    TEST_ASSERT_EQUAL_UINT16(0, tt.beatPhaseMs(1000));
}

// ─── Activation ──────────────────────────────────────────────────────────────

void test_tap_tempo_single_tap_not_active() {
    TapTempo tt;
    tt.tap(1000);
    TEST_ASSERT_FALSE(tt.isActive());
    TEST_ASSERT_EQUAL_UINT16(0, tt.beatIntervalMs());
}

void test_tap_tempo_two_taps_becomes_active() {
    TapTempo tt;
    tt.tap(0);
    tt.tap(500);
    TEST_ASSERT_TRUE(tt.isActive());
}

void test_tap_tempo_two_taps_interval_correct() {
    TapTempo tt;
    tt.tap(0);
    tt.tap(500);
    TEST_ASSERT_EQUAL_UINT16(500, tt.beatIntervalMs());
}

// ─── Gap reset / re-anchor ───────────────────────────────────────────────────

void test_tap_tempo_large_gap_no_tempo_resets() {
    // No established tempo: large gap on second tap → stays inactive.
    TapTempo tt;
    tt.tap(0);
    tt.tap(TapTempo::kMaxGapMs + 1);  // gap too large, no tempo yet
    TEST_ASSERT_FALSE(tt.isActive());
    TEST_ASSERT_EQUAL_UINT16(0, tt.beatIntervalMs());
}

void test_tap_tempo_large_gap_reanchors_if_active() {
    // Established tempo: large gap re-anchors the phase but stays active.
    TapTempo tt;
    tt.tap(0);
    tt.tap(500);  // active, interval=500
    TEST_ASSERT_TRUE(tt.isActive());
    TimeMs reanchorNow = 500 + TapTempo::kMaxGapMs + 1;
    tt.tap(reanchorNow);
    TEST_ASSERT_TRUE(tt.isActive());
    TEST_ASSERT_EQUAL_UINT16(500, tt.beatIntervalMs());
    // Phase should be 0 immediately after the re-anchor tap.
    TEST_ASSERT_EQUAL_UINT16(0, tt.beatPhaseMs(reanchorNow));
}

void test_tap_tempo_tap_exactly_at_max_gap_resets() {
    TapTempo tt;
    tt.tap(0);
    tt.tap(TapTempo::kMaxGapMs);  // exactly at limit — should reset (< not <=)
    TEST_ASSERT_FALSE(tt.isActive());
}

void test_tap_tempo_tap_just_under_max_gap_keeps_active() {
    TapTempo tt;
    // 1000ms = 60 BPM — well within valid range and under kMaxGapMs
    tt.tap(0);
    tt.tap(1000);
    TEST_ASSERT_TRUE(tt.isActive());
}

// ─── BPM range filter ────────────────────────────────────────────────────────

void test_tap_tempo_rejects_too_slow_tap() {
    // 2500ms interval = 24 BPM, below kTapMinBpm (30). Should not become active.
    TapTempo tt;
    tt.tap(0);
    tt.tap(2500);
    TEST_ASSERT_FALSE(tt.isActive());
    TEST_ASSERT_EQUAL_UINT16(0, tt.beatIntervalMs());
}

void test_tap_tempo_rejects_too_fast_tap() {
    // 100ms interval = 600 BPM, above kTapMaxBpm (240). Should not become active.
    TapTempo tt;
    tt.tap(0);
    tt.tap(100);
    TEST_ASSERT_FALSE(tt.isActive());
    TEST_ASSERT_EQUAL_UINT16(0, tt.beatIntervalMs());
}

void test_tap_tempo_accepts_min_bpm_boundary() {
    // 2000ms = exactly 30 BPM (kTapMinBpm). Should become active.
    TapTempo tt;
    tt.tap(0);
    tt.tap(2000);
    TEST_ASSERT_TRUE(tt.isActive());
}

void test_tap_tempo_accepts_max_bpm_boundary() {
    // 250ms = exactly 240 BPM (kTapMaxBpm). Should become active.
    TapTempo tt;
    tt.tap(0);
    tt.tap(250);
    TEST_ASSERT_TRUE(tt.isActive());
}

void test_tap_tempo_invalid_tap_does_not_reset_active_tempo() {
    // Establish a tempo, then tap out-of-range. Tempo should be preserved.
    TapTempo tt;
    tt.tap(0);
    tt.tap(500);  // 120 BPM, active
    TEST_ASSERT_TRUE(tt.isActive());
    tt.tap(600);  // 100ms later = 600 BPM, too fast — ignored
    TEST_ASSERT_TRUE(tt.isActive());
    TEST_ASSERT_UINT16_WITHIN(5, 500, tt.beatIntervalMs());
}

// ─── Smoothing ───────────────────────────────────────────────────────────────

void test_tap_tempo_smoothing_converges() {
    TapTempo tt;
    // Tap at 500ms intervals several times. Interval should stay at 500.
    for (int i = 0; i <= 8; ++i)
        tt.tap(i * 500);
    TEST_ASSERT_TRUE(tt.isActive());
    // After many consistent taps, smoothed interval should be very close to 500.
    TEST_ASSERT_UINT16_WITHIN(5, 500, tt.beatIntervalMs());
}

// ─── Beat phase ──────────────────────────────────────────────────────────────

void test_tap_tempo_beat_phase_zero_immediately_after_tap() {
    TapTempo tt;
    tt.tap(0);
    tt.tap(500);
    TEST_ASSERT_EQUAL_UINT16(0, tt.beatPhaseMs(500));
}

void test_tap_tempo_beat_phase_progresses() {
    TapTempo tt;
    tt.tap(0);
    tt.tap(500);
    // 100ms after last tap = 100ms into the 500ms beat
    TEST_ASSERT_EQUAL_UINT16(100, tt.beatPhaseMs(600));
}

void test_tap_tempo_beat_phase_wraps_at_interval() {
    TapTempo tt;
    tt.tap(0);
    tt.tap(500);
    // 600ms after last tap = 100ms into next beat (600 % 500 = 100)
    TEST_ASSERT_EQUAL_UINT16(100, tt.beatPhaseMs(1100));
}

// ─── reset() ─────────────────────────────────────────────────────────────────

void test_tap_tempo_reset_clears_state() {
    TapTempo tt;
    tt.tap(0);
    tt.tap(500);
    TEST_ASSERT_TRUE(tt.isActive());
    tt.reset();
    TEST_ASSERT_FALSE(tt.isActive());
    TEST_ASSERT_EQUAL_UINT16(0, tt.beatIntervalMs());
    TEST_ASSERT_EQUAL_UINT16(0, tt.beatPhaseMs(1000));
}

// ─── setFromPacket() ─────────────────────────────────────────────────────────

void test_tap_tempo_set_from_packet_becomes_active() {
    TapTempo tt;
    tt.setFromPacket(500, 10000, 100);
    TEST_ASSERT_TRUE(tt.isActive());
    TEST_ASSERT_EQUAL_UINT16(500, tt.beatIntervalMs());
}

void test_tap_tempo_set_from_packet_phase_correct() {
    TapTempo tt;
    // Packet says: at t=10000, we were 100ms into a 500ms beat.
    // Last beat was at 10000 - 100 = 9900.
    tt.setFromPacket(500, 10000, 100);
    // At t=10200, elapsed since last beat = 10200 - 9900 = 300
    TEST_ASSERT_EQUAL_UINT16(300, tt.beatPhaseMs(10200));
}

void test_tap_tempo_set_from_packet_phase_wraps() {
    TapTempo tt;
    // Last beat at 9900 (= 10000 - 100), interval 500.
    tt.setFromPacket(500, 10000, 100);
    // At t=10500: elapsed = 10500 - 9900 = 600; 600 % 500 = 100
    TEST_ASSERT_EQUAL_UINT16(100, tt.beatPhaseMs(10500));
}

// ---------------------------------------------------------------------------

void tap_tempo_tests() {
    RUN_TEST(test_tap_tempo_initial_not_active);
    RUN_TEST(test_tap_tempo_single_tap_not_active);
    RUN_TEST(test_tap_tempo_two_taps_becomes_active);
    RUN_TEST(test_tap_tempo_two_taps_interval_correct);
    RUN_TEST(test_tap_tempo_large_gap_no_tempo_resets);
    RUN_TEST(test_tap_tempo_large_gap_reanchors_if_active);
    RUN_TEST(test_tap_tempo_tap_exactly_at_max_gap_resets);
    RUN_TEST(test_tap_tempo_tap_just_under_max_gap_keeps_active);
    RUN_TEST(test_tap_tempo_rejects_too_slow_tap);
    RUN_TEST(test_tap_tempo_rejects_too_fast_tap);
    RUN_TEST(test_tap_tempo_accepts_min_bpm_boundary);
    RUN_TEST(test_tap_tempo_accepts_max_bpm_boundary);
    RUN_TEST(test_tap_tempo_invalid_tap_does_not_reset_active_tempo);
    RUN_TEST(test_tap_tempo_smoothing_converges);
    RUN_TEST(test_tap_tempo_beat_phase_zero_immediately_after_tap);
    RUN_TEST(test_tap_tempo_beat_phase_progresses);
    RUN_TEST(test_tap_tempo_beat_phase_wraps_at_interval);
    RUN_TEST(test_tap_tempo_reset_clears_state);
    RUN_TEST(test_tap_tempo_set_from_packet_becomes_active);
    RUN_TEST(test_tap_tempo_set_from_packet_phase_correct);
    RUN_TEST(test_tap_tempo_set_from_packet_phase_wraps);
}
