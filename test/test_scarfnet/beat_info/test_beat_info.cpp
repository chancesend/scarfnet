#include <unity.h>
#include <beat_info.h>

using Scarfnet::BeatInfo;
using Scarfnet::BarBeat;

// Helpers
// 500 ms/beat, 4 beats/bar — a convenient round-number fixture.
static BeatInfo make(uint16_t intervalMs, uint16_t phaseMs, uint16_t beatNumber) {
    return BeatInfo(intervalMs, phaseMs, beatNumber);
}
static constexpr uint16_t kInterval = 500;

// ─── Inactive (default-constructed) ──────────────────────────────────────────

void test_beat_info_inactive_is_not_active() {
    BeatInfo b;
    TEST_ASSERT_FALSE(b.isActive());
}

void test_beat_info_inactive_returns_zero_for_all() {
    BeatInfo b;
    TEST_ASSERT_EQUAL_UINT8(0,  b.beatFrac8());
    TEST_ASSERT_EQUAL_UINT8(0,  b.beatInBar());
    TEST_ASSERT_EQUAL_UINT16(0, b.barNumber());
    TEST_ASSERT_EQUAL_UINT32(0, b.barPhaseMs());
    TEST_ASSERT_EQUAL_UINT8(0,  b.barFrac8());
    TEST_ASSERT_FALSE(b.isOnBeat());
    TEST_ASSERT_FALSE(b.isOnBar());
    TEST_ASSERT_FALSE(b.isOnPhrase(16));
    TEST_ASSERT_EQUAL_UINT8(0, b.flashBrightness());
    TEST_ASSERT_EQUAL_UINT8(0, b.phraseFlashBrightness(16));
}

void test_beat_info_inactive_position_is_zero() {
    BeatInfo b;
    BarBeat pos = b.position();
    TEST_ASSERT_EQUAL_UINT16(0, pos.bar);
    TEST_ASSERT_EQUAL_UINT8(0,  pos.beat);
    TEST_ASSERT_EQUAL_UINT8(0,  pos.subdivision);
    TEST_ASSERT_EQUAL_UINT8(0,  pos.beatFrac8);
}

// ─── isActive ────────────────────────────────────────────────────────────────

void test_beat_info_active_when_interval_nonzero() {
    TEST_ASSERT_TRUE(make(kInterval, 0, 0).isActive());
}

// ─── isOnBeat / flashBrightness ──────────────────────────────────────────────

void test_beat_info_is_on_beat_at_phase_zero() {
    TEST_ASSERT_TRUE(make(kInterval, 0, 0).isOnBeat());
}

void test_beat_info_is_on_beat_within_window() {
    TEST_ASSERT_TRUE(make(kInterval, 59, 0).isOnBeat(60));
}

void test_beat_info_not_on_beat_at_window_boundary() {
    TEST_ASSERT_FALSE(make(kInterval, 60, 0).isOnBeat(60));
}

void test_beat_info_not_on_beat_past_window() {
    TEST_ASSERT_FALSE(make(kInterval, 100, 0).isOnBeat(60));
}

void test_beat_info_flash_brightness_peaks_at_onset() {
    TEST_ASSERT_EQUAL_UINT8(255, make(kInterval, 0, 0).flashBrightness(100));
}

void test_beat_info_flash_brightness_midpoint() {
    // phaseMs=50, window=100 → 255 - 50*255/100 = 255 - 127 = 128
    TEST_ASSERT_EQUAL_UINT8(128, make(kInterval, 50, 0).flashBrightness(100));
}

void test_beat_info_flash_brightness_zero_outside_window() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 100, 0).flashBrightness(100));
}

// ─── beatFrac8 ───────────────────────────────────────────────────────────────

void test_beat_info_beat_frac8_at_onset() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 0).beatFrac8());
}

void test_beat_info_beat_frac8_at_midpoint() {
    // phaseMs=250, interval=500 → 250*255/500 = 127
    TEST_ASSERT_EQUAL_UINT8(127, make(kInterval, 250, 0).beatFrac8());
}

void test_beat_info_beat_frac8_near_end() {
    // phaseMs=499, interval=500 → 499*255/500 = 254
    TEST_ASSERT_EQUAL_UINT8(254, make(kInterval, 499, 0).beatFrac8());
}

// ─── beatInBar ───────────────────────────────────────────────────────────────

void test_beat_info_beat_in_bar_downbeat() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 0).beatInBar());
}

void test_beat_info_beat_in_bar_second_beat() {
    TEST_ASSERT_EQUAL_UINT8(1, make(kInterval, 0, 1).beatInBar());
}

void test_beat_info_beat_in_bar_wraps_at_bar_boundary() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 4).beatInBar());  // beat 4 → bar 1 downbeat
    TEST_ASSERT_EQUAL_UINT8(3, make(kInterval, 0, 7).beatInBar());  // beat 7 → beat 3 of bar 1
}

// ─── barNumber ───────────────────────────────────────────────────────────────

void test_beat_info_bar_number_first_bar() {
    TEST_ASSERT_EQUAL_UINT16(0, make(kInterval, 0, 0).barNumber());
    TEST_ASSERT_EQUAL_UINT16(0, make(kInterval, 0, 3).barNumber());
}

void test_beat_info_bar_number_increments_on_bar_boundary() {
    TEST_ASSERT_EQUAL_UINT16(1, make(kInterval, 0, 4).barNumber());
    TEST_ASSERT_EQUAL_UINT16(2, make(kInterval, 0, 8).barNumber());
}

// ─── isOnBar ─────────────────────────────────────────────────────────────────

void test_beat_info_is_on_bar_at_downbeat() {
    // beat 0, phaseMs=0 → bar downbeat
    TEST_ASSERT_TRUE(make(kInterval, 0, 0).isOnBar());
}

void test_beat_info_is_on_bar_at_subsequent_bar() {
    // beat 4 (bar 1 downbeat), phaseMs=10
    TEST_ASSERT_TRUE(make(kInterval, 10, 4).isOnBar());
}

void test_beat_info_not_on_bar_mid_beat() {
    // beat 0, but phaseMs past window
    TEST_ASSERT_FALSE(make(kInterval, 61, 0).isOnBar(4, 60));
}

void test_beat_info_not_on_bar_non_downbeat() {
    // beat 1 of bar 0 — not a downbeat even at phaseMs=0
    TEST_ASSERT_FALSE(make(kInterval, 0, 1).isOnBar());
    TEST_ASSERT_FALSE(make(kInterval, 0, 2).isOnBar());
    TEST_ASSERT_FALSE(make(kInterval, 0, 3).isOnBar());
}

// ─── barPhaseMs ──────────────────────────────────────────────────────────────

void test_beat_info_bar_phase_ms_at_downbeat() {
    // beat 0, phaseMs=0 → 0ms into bar
    TEST_ASSERT_EQUAL_UINT32(0, make(kInterval, 0, 0).barPhaseMs());
}

void test_beat_info_bar_phase_ms_mid_bar() {
    // beat 2 of bar 0, phaseMs=100 → 2*500 + 100 = 1100
    TEST_ASSERT_EQUAL_UINT32(1100, make(kInterval, 100, 2).barPhaseMs());
}

void test_beat_info_bar_phase_ms_resets_at_bar_boundary() {
    // beat 4 (bar 1 downbeat), phaseMs=50 → 0*500 + 50 = 50
    TEST_ASSERT_EQUAL_UINT32(50, make(kInterval, 50, 4).barPhaseMs());
}

// ─── barFrac8 ────────────────────────────────────────────────────────────────

void test_beat_info_bar_frac8_at_downbeat() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 0).barFrac8());
}

void test_beat_info_bar_frac8_at_bar_midpoint() {
    // beat 2 of 4, phaseMs=0 → barPhase=1000, barMs=2000 → 1000*255/2000=127
    TEST_ASSERT_EQUAL_UINT8(127, make(kInterval, 0, 2).barFrac8());
}

void test_beat_info_bar_frac8_resets_each_bar() {
    // bar 1 downbeat should give 0
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 4).barFrac8());
}

// ─── isOnPhrase / phraseFlashBrightness ──────────────────────────────────────

void test_beat_info_is_on_phrase_at_start() {
    TEST_ASSERT_TRUE(make(kInterval, 0, 0).isOnPhrase(16));
}

void test_beat_info_is_on_phrase_at_boundary() {
    TEST_ASSERT_TRUE(make(kInterval, 0, 16).isOnPhrase(16));
    TEST_ASSERT_TRUE(make(kInterval, 0, 32).isOnPhrase(16));
}

void test_beat_info_not_on_phrase_mid_phrase() {
    TEST_ASSERT_FALSE(make(kInterval, 0, 1).isOnPhrase(16));
    TEST_ASSERT_FALSE(make(kInterval, 0, 8).isOnPhrase(16));
    TEST_ASSERT_FALSE(make(kInterval, 0, 15).isOnPhrase(16));
}

void test_beat_info_not_on_phrase_outside_window() {
    TEST_ASSERT_FALSE(make(kInterval, 61, 0).isOnPhrase(16, 60));
}

void test_beat_info_phrase_flash_brightness_peaks_at_boundary() {
    TEST_ASSERT_EQUAL_UINT8(255, make(kInterval, 0, 16).phraseFlashBrightness(16, 100));
}

void test_beat_info_phrase_flash_brightness_zero_off_boundary() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 8).phraseFlashBrightness(16, 100));
}

// ─── position() / BarBeat ────────────────────────────────────────────────────

void test_beat_info_position_at_bar_downbeat() {
    BarBeat pos = make(kInterval, 0, 0).position();
    TEST_ASSERT_EQUAL_UINT16(0, pos.bar);
    TEST_ASSERT_EQUAL_UINT8(0,  pos.beat);
    TEST_ASSERT_EQUAL_UINT8(0,  pos.subdivision);
    TEST_ASSERT_EQUAL_UINT8(0,  pos.beatFrac8);
}

void test_beat_info_position_mid_phrase() {
    // beat 6 (bar 1, beat 2), phaseMs=250 (midpoint of beat)
    BarBeat pos = make(kInterval, 250, 6).position();
    TEST_ASSERT_EQUAL_UINT16(1,   pos.bar);    // beat 6 / 4 = bar 1
    TEST_ASSERT_EQUAL_UINT8(2,    pos.beat);   // beat 6 % 4 = 2
    TEST_ASSERT_EQUAL_UINT8(1,    pos.subdivision); // 250 / (500/2) = 1 (second 8th note)
    TEST_ASSERT_EQUAL_UINT8(127,  pos.beatFrac8);   // 250*255/500 = 127
}

void test_beat_info_position_16th_note_subdivisions() {
    // phaseMs=100 of 500, subdivisionsPerBeat=4 → subDiv index = 100/(500/4) = 100/125 = 0
    BarBeat pos = make(kInterval, 100, 0).position(4, 4);
    TEST_ASSERT_EQUAL_UINT8(0, pos.subdivision);  // first 16th note slot

    // phaseMs=200 → 200/125 = 1
    BarBeat pos2 = make(kInterval, 200, 0).position(4, 4);
    TEST_ASSERT_EQUAL_UINT8(1, pos2.subdivision);
}

// ---------------------------------------------------------------------------

void beat_info_tests() {
    // Inactive
    RUN_TEST(test_beat_info_inactive_is_not_active);
    RUN_TEST(test_beat_info_inactive_returns_zero_for_all);
    RUN_TEST(test_beat_info_inactive_position_is_zero);

    // isActive
    RUN_TEST(test_beat_info_active_when_interval_nonzero);

    // isOnBeat / flashBrightness
    RUN_TEST(test_beat_info_is_on_beat_at_phase_zero);
    RUN_TEST(test_beat_info_is_on_beat_within_window);
    RUN_TEST(test_beat_info_not_on_beat_at_window_boundary);
    RUN_TEST(test_beat_info_not_on_beat_past_window);
    RUN_TEST(test_beat_info_flash_brightness_peaks_at_onset);
    RUN_TEST(test_beat_info_flash_brightness_midpoint);
    RUN_TEST(test_beat_info_flash_brightness_zero_outside_window);

    // beatFrac8
    RUN_TEST(test_beat_info_beat_frac8_at_onset);
    RUN_TEST(test_beat_info_beat_frac8_at_midpoint);
    RUN_TEST(test_beat_info_beat_frac8_near_end);

    // beatInBar
    RUN_TEST(test_beat_info_beat_in_bar_downbeat);
    RUN_TEST(test_beat_info_beat_in_bar_second_beat);
    RUN_TEST(test_beat_info_beat_in_bar_wraps_at_bar_boundary);

    // barNumber
    RUN_TEST(test_beat_info_bar_number_first_bar);
    RUN_TEST(test_beat_info_bar_number_increments_on_bar_boundary);

    // isOnBar
    RUN_TEST(test_beat_info_is_on_bar_at_downbeat);
    RUN_TEST(test_beat_info_is_on_bar_at_subsequent_bar);
    RUN_TEST(test_beat_info_not_on_bar_mid_beat);
    RUN_TEST(test_beat_info_not_on_bar_non_downbeat);

    // barPhaseMs
    RUN_TEST(test_beat_info_bar_phase_ms_at_downbeat);
    RUN_TEST(test_beat_info_bar_phase_ms_mid_bar);
    RUN_TEST(test_beat_info_bar_phase_ms_resets_at_bar_boundary);

    // barFrac8
    RUN_TEST(test_beat_info_bar_frac8_at_downbeat);
    RUN_TEST(test_beat_info_bar_frac8_at_bar_midpoint);
    RUN_TEST(test_beat_info_bar_frac8_resets_each_bar);

    // isOnPhrase / phraseFlashBrightness
    RUN_TEST(test_beat_info_is_on_phrase_at_start);
    RUN_TEST(test_beat_info_is_on_phrase_at_boundary);
    RUN_TEST(test_beat_info_not_on_phrase_mid_phrase);
    RUN_TEST(test_beat_info_not_on_phrase_outside_window);
    RUN_TEST(test_beat_info_phrase_flash_brightness_peaks_at_boundary);
    RUN_TEST(test_beat_info_phrase_flash_brightness_zero_off_boundary);

    // position() / BarBeat
    RUN_TEST(test_beat_info_position_at_bar_downbeat);
    RUN_TEST(test_beat_info_position_mid_phrase);
    RUN_TEST(test_beat_info_position_16th_note_subdivisions);
}
