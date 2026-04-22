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

void test_beat_info_inactive_returns_start_for_all_envelopes() {
    BeatInfo b;
    TEST_ASSERT_EQUAL_UINT8(0,  b.frac8());
    TEST_ASSERT_EQUAL_UINT8(0,  b.beatInBar());
    TEST_ASSERT_EQUAL_UINT16(0, b.barNumber());
    TEST_ASSERT_EQUAL_UINT32(0, b.barPhaseMs());
    TEST_ASSERT_EQUAL_UINT8(0,  b.frac8());
    TEST_ASSERT_FALSE(b.isOnBeat());
    TEST_ASSERT_FALSE(b.isOnBar());
    TEST_ASSERT_FALSE(b.isOnPhrase(16));
    TEST_ASSERT_FALSE(b.isLastBeatOfPhrase(16));
    TEST_ASSERT_EQUAL_UINT8(0,  b.phraseFrac8(16));
    // All envelopes return `start` when inactive
    TEST_ASSERT_EQUAL_UINT8(255, b.sawTime());           // default start=255
    TEST_ASSERT_EQUAL_UINT8(0,   b.saw());               // default start=0
    TEST_ASSERT_EQUAL_UINT8(0,   b.triangle());          // default start=0
    TEST_ASSERT_EQUAL_UINT8(0,   b.sin());               // default start=0
    TEST_ASSERT_EQUAL_UINT8(0,   b.square());            // default start=0
    TEST_ASSERT_EQUAL_UINT8(0,   b.expDecay());          // default start=0
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

// ─── tempo ───────────────────────────────────────────────────────────────────

void test_beat_info_tempo_inactive_is_zero() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, BeatInfo().tempo());
}

void test_beat_info_tempo_120bpm() {
    // 120 BPM = 500ms interval
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, make(500, 0, 0).tempo());
}

void test_beat_info_tempo_60bpm() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, make(1000, 0, 0).tempo());
}

void test_beat_info_tempo_140bpm() {
    // 60000 / 428 = 140.187 BPM — check it's in the right ballpark
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 140.0f, make(428, 0, 0).tempo());
}

// ─── isOnBeat ────────────────────────────────────────────────────────────────

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

// ─── beatFrac8 ───────────────────────────────────────────────────────────────

void test_beat_info_frac8_at_onset() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 0).frac8());
}

void test_beat_info_frac8_at_midpoint() {
    // phaseMs=250, interval=500 → 250*255/500 = 127
    TEST_ASSERT_EQUAL_UINT8(127, make(kInterval, 250, 0).frac8());
}

void test_beat_info_frac8_near_end() {
    // phaseMs=499, interval=500 → 499*255/500 = 254
    TEST_ASSERT_EQUAL_UINT8(254, make(kInterval, 499, 0).frac8());
}

// ─── beatSawTime ─────────────────────────────────────────────────────────────

void test_beat_saw_time_start_at_onset() {
    // start=255, end=0, windowMs=100; at phaseMs=0 → 255
    TEST_ASSERT_EQUAL_UINT8(255, make(kInterval, 0, 0).sawTime(100));
}

void test_beat_saw_time_midpoint_decay() {
    // start=255, end=0, windowMs=100, phaseMs=50 → 255 + (0-255)*50/100 = 128
    // (integer division truncates toward zero: -12750/100 = -127, so 255-127=128)
    TEST_ASSERT_EQUAL_UINT8(128, make(kInterval, 50, 0).sawTime(100));
}

void test_beat_saw_time_end_at_boundary() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 100, 0).sawTime(100));
}

void test_beat_saw_time_holds_end_past_window() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 400, 0).sawTime(100));
}

void test_beat_saw_time_attack_direction() {
    // start=0, end=255, windowMs=100, phaseMs=50 → 0 + (255)*50/100 = 127
    TEST_ASSERT_EQUAL_UINT8(127, make(kInterval, 50, 0).sawTime(100, 0, 255));
}

void test_beat_saw_time_custom_range() {
    // start=100, end=200, windowMs=100: onset=100, boundary=200
    TEST_ASSERT_EQUAL_UINT8(100, make(kInterval, 0,   0).sawTime(100, 100, 200));
    TEST_ASSERT_EQUAL_UINT8(200, make(kInterval, 100, 0).sawTime(100, 100, 200));
}

// ─── beatSaw ─────────────────────────────────────────────────────────────────

void test_beat_saw_start_at_onset() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 0).saw());
}

void test_beat_saw_full_beat_midpoint() {
    // start=0, end=255, duty=1.0; phaseMs=250/500 → 127
    TEST_ASSERT_EQUAL_UINT8(127, make(kInterval, 250, 0).saw());
}

void test_beat_saw_holds_end_after_duty_window() {
    // duty=0.5 → activeMs=250; phaseMs=300 → past window → end=255
    TEST_ASSERT_EQUAL_UINT8(255, make(kInterval, 300, 0).saw(0, 255, 0.5f));
}

void test_beat_saw_falling_direction() {
    // start=255, end=0; at onset → 255, midpoint → 128
    TEST_ASSERT_EQUAL_UINT8(255, make(kInterval, 0,   0).saw(255, 0));
    TEST_ASSERT_EQUAL_UINT8(128, make(kInterval, 250, 0).saw(255, 0));
}

void test_beat_saw_half_duty_midpoint() {
    // duty=0.5 → activeMs=250; phaseMs=125 → halfway → 127
    TEST_ASSERT_EQUAL_UINT8(127, make(kInterval, 125, 0).saw(0, 255, 0.5f));
}

void test_beat_saw_phase_shifts_onset() {
    // phase=0.5 shifts by half a beat; at phaseMs=0 the effective position is 250ms into a 500ms window
    // → midpoint of the saw → 127
    TEST_ASSERT_EQUAL_UINT8(127, make(kInterval, 0, 0).saw(0, 255, 1.0f, 0.5f));
}

void test_beat_saw_phase_wraps() {
    // phase=0.5, phaseMs=250 → effectiveMs = (250 + 250) % 500 = 0 → start=0
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 250, 0).saw(0, 255, 1.0f, 0.5f));
}

// ─── beatTriangle ────────────────────────────────────────────────────────────

void test_beat_triangle_start_at_onset() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 0).triangle());
}

void test_beat_triangle_peaks_at_midbeat() {
    // phaseMs=250/500 → frac≈127, tri=254 → lerp → 253
    TEST_ASSERT_EQUAL_UINT8(253, make(kInterval, 250, 0).triangle());
}

void test_beat_triangle_start_at_end() {
    // phaseMs=499 → frac≈254, tri=2 → lerp → 1; holds start outside active window at duty<1
    TEST_ASSERT_EQUAL_UINT8(1, make(kInterval, 499, 0).triangle());
}

void test_beat_triangle_half_duty_holds_start_past_window() {
    // duty=0.5 → activeMs=250; phaseMs=300 → past window → start=0
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 300, 0).triangle(0, 255, 0.5f));
}

void test_beat_triangle_phase_shifts_peak() {
    // phase=0.25 → effectiveMs = phaseMs + 125; at phaseMs=125, effectiveMs=250 → peak → 253
    TEST_ASSERT_EQUAL_UINT8(253, make(kInterval, 125, 0).triangle(0, 255, 1.0f, 0.25f));
}

// ─── beatSin ─────────────────────────────────────────────────────────────────

void test_beat_sin_lo_at_onset() {
    // sin8(0)=128, s=(128-128)*2=0 → lo
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 0).sin());
}

void test_beat_sin_near_peak_at_midbeat() {
    // phaseMs=250 → frac=127, frac>>1=63, sin8(63)≈253, s=(253-128)*2=250
    // lerp8by8(0, 255, 250) ≈ 249
    TEST_ASSERT_UINT8_WITHIN(10, 249, make(kInterval, 250, 0).sin());
}

void test_beat_sin_near_lo_at_end() {
    // frac=254, frac>>1=127, sin8(127)≈129, s=(129-128)*2=2 → lerp8by8(0,255,2)≈2
    TEST_ASSERT_UINT8_WITHIN(5, 2, make(kInterval, 499, 0).sin());
}

// ─── beatSquare ──────────────────────────────────────────────────────────────

void test_beat_square_hi_in_duty_window() {
    // duty=128 (50%); frac=0 → hi
    TEST_ASSERT_EQUAL_UINT8(255, make(kInterval, 0, 0).square(128));
}

void test_beat_square_lo_past_duty() {
    // frac=128 → not < 128 → lo
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 251, 0).square(128));
}

void test_beat_square_lo_hi_range() {
    TEST_ASSERT_EQUAL_UINT8(200, make(kInterval, 0, 0).square(200, 50, 200));
    TEST_ASSERT_EQUAL_UINT8(50,  make(kInterval, 400, 0).square(128, 50, 200));
}

// ─── beatExpDecay ────────────────────────────────────────────────────────────

void test_beat_exp_decay_hi_at_onset() {
    // frac=0, remain=255, scale8(255,255)≈255 → hi
    TEST_ASSERT_UINT8_WITHIN(2, 255, make(kInterval, 0, 0).expDecay());
}

void test_beat_exp_decay_below_linear_at_midpoint() {
    // frac≈127, remain=128, scale8(128,128)=64 → lerp8by8(0,255,64)≈63
    // beatSawDuty (linear) at midpoint gives ~127; exp should be lower (squashed toward end=255)
    uint8_t expVal    = make(kInterval, 250, 0).expDecay();
    uint8_t linearVal = make(kInterval, 250, 0).saw(0, 255, 1.0f);
    TEST_ASSERT_LESS_THAN(linearVal, expVal);  // exp stays higher longer (more energy at start)
}

void test_beat_exp_decay_near_lo_at_end() {
    // frac=254, remain=1, scale8(1,1)=0 → lo
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 499, 0).expDecay());
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
    TEST_ASSERT_TRUE(make(kInterval, 0, 0).isOnBar());
}

void test_beat_info_is_on_bar_at_subsequent_bar() {
    TEST_ASSERT_TRUE(make(kInterval, 10, 4).isOnBar());
}

void test_beat_info_not_on_bar_mid_beat() {
    TEST_ASSERT_FALSE(make(kInterval, 61, 0).isOnBar(4, 60));
}

void test_beat_info_not_on_bar_non_downbeat() {
    TEST_ASSERT_FALSE(make(kInterval, 0, 1).isOnBar());
    TEST_ASSERT_FALSE(make(kInterval, 0, 2).isOnBar());
    TEST_ASSERT_FALSE(make(kInterval, 0, 3).isOnBar());
}

// ─── barPhaseMs ──────────────────────────────────────────────────────────────

void test_beat_info_bar_phase_ms_at_downbeat() {
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
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 0).frac8());
}

void test_beat_info_bar_frac8_at_midpoint() {
    // beat 2 of 4, phaseMs=0 → barPhase=1000, barMs=2000 → 1000*255/2000=127
    TEST_ASSERT_EQUAL_UINT8(127, make(kInterval, 0, 2).frac8());
}

void test_beat_info_bar_frac8_resets_each_bar() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 4).frac8());
}

// ─── isOnPhrase / isLastBeatOfPhrase / phraseFrac8 ───────────────────────────

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

void test_beat_info_is_last_beat_of_phrase() {
    TEST_ASSERT_TRUE(make(kInterval, 0, 15).isLastBeatOfPhrase(16));
    TEST_ASSERT_TRUE(make(kInterval, 0, 31).isLastBeatOfPhrase(16));
}

void test_beat_info_not_last_beat_of_phrase() {
    TEST_ASSERT_FALSE(make(kInterval, 0, 14).isLastBeatOfPhrase(16));
    TEST_ASSERT_FALSE(make(kInterval, 0, 16).isLastBeatOfPhrase(16));  // first beat of next phrase
    TEST_ASSERT_FALSE(make(kInterval, 0, 0).isLastBeatOfPhrase(16));
}

void test_beat_info_phrase_frac8_at_start() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 0).phraseFrac8(16));
}

void test_beat_info_phrase_frac8_at_midpoint() {
    // beat 8 of 16, phaseMs=0 → phase=8*500=4000, phraseMs=16*500=8000 → 4000*255/8000=127
    TEST_ASSERT_EQUAL_UINT8(127, make(kInterval, 0, 8).phraseFrac8(16));
}

void test_beat_info_phrase_frac8_resets_at_boundary() {
    TEST_ASSERT_EQUAL_UINT8(0, make(kInterval, 0, 16).phraseFrac8(16));
}

// ─── phraseFrac16 ────────────────────────────────────────────────────────────

void test_beat_info_phrase_frac16_inactive() {
    TEST_ASSERT_EQUAL_UINT16(0, BeatInfo().phraseFrac16(16));
}

void test_beat_info_phrase_frac16_at_start() {
    TEST_ASSERT_EQUAL_UINT16(0, make(kInterval, 0, 0).phraseFrac16(16));
}

void test_beat_info_phrase_frac16_at_midpoint() {
    // beat 8 of 16 → phase=4000, phraseMs=8000 → 4000*65535/8000 = 32767
    TEST_ASSERT_EQUAL_UINT16(32767, make(kInterval, 0, 8).phraseFrac16(16));
}

void test_beat_info_phrase_frac16_resets_at_boundary() {
    TEST_ASSERT_EQUAL_UINT16(0, make(kInterval, 0, 16).phraseFrac16(16));
}

void test_beat_info_phrase_frac16_higher_precision_than_frac8() {
    // At beat 1 of 16, phaseMs=0: frac8 = 1*500*255/8000 = 15; frac16 = 1*500*65535/8000 = 4095
    TEST_ASSERT_EQUAL_UINT8(15,   make(kInterval, 0, 1).phraseFrac8(16));
    TEST_ASSERT_EQUAL_UINT16(4095, make(kInterval, 0, 1).phraseFrac16(16));
}

// ─── phraseView ──────────────────────────────────────────────────────────────

void test_beat_info_phrase_view_inactive_returns_inactive() {
    TEST_ASSERT_FALSE(BeatInfo().phraseView(16).isActive());
}

void test_beat_info_phrase_view_has_virtual_interval() {
    BeatInfo pv = make(kInterval, 0, 0).phraseView(16);
    TEST_ASSERT_EQUAL_UINT16(10000, pv.intervalMs);
}

void test_beat_info_phrase_view_phase_zero_at_phrase_start() {
    BeatInfo pv = make(kInterval, 0, 0).phraseView(16);
    TEST_ASSERT_EQUAL_UINT16(0, pv.phaseMs);
}

void test_beat_info_phrase_view_phase_at_midpoint() {
    // beat 8 of 16 → scaled to 5000 of 10000
    BeatInfo pv = make(kInterval, 0, 8).phraseView(16);
    TEST_ASSERT_EQUAL_UINT16(5000, pv.phaseMs);
}

void test_beat_info_phrase_view_beatnumber_is_phrase_count() {
    TEST_ASSERT_EQUAL_UINT16(0, make(kInterval, 0, 0).phraseView(16).beatNumber);
    TEST_ASSERT_EQUAL_UINT16(1, make(kInterval, 0, 16).phraseView(16).beatNumber);
    TEST_ASSERT_EQUAL_UINT16(2, make(kInterval, 0, 32).phraseView(16).beatNumber);
}

void test_beat_info_phrase_view_saw_rising() {
    // phraseView(16).beatSaw() at phrase midpoint → ~127
    BeatInfo pv = make(kInterval, 0, 8).phraseView(16);
    TEST_ASSERT_UINT8_WITHIN(2, 127, pv.saw());
}

void test_beat_info_phrase_view_triangle_peaks_at_mid() {
    // beatTriangle on phraseView(16) peaks at phrase midpoint (beat 8)
    BeatInfo mid  = make(kInterval, 0, 8).phraseView(16);
    BeatInfo early = make(kInterval, 0, 2).phraseView(16);
    TEST_ASSERT_GREATER_THAN(early.triangle(), mid.triangle());
}

void test_beat_info_phrase_view_sin_peaks_at_mid() {
    BeatInfo mid   = make(kInterval, 0, 8).phraseView(16);
    BeatInfo start = make(kInterval, 0, 0).phraseView(16);
    TEST_ASSERT_GREATER_THAN(start.sin(), mid.sin());
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
    TEST_ASSERT_EQUAL_UINT16(1,   pos.bar);
    TEST_ASSERT_EQUAL_UINT8(2,    pos.beat);
    TEST_ASSERT_EQUAL_UINT8(1,    pos.subdivision); // 250 / (500/2) = 1 (second 8th note)
    TEST_ASSERT_EQUAL_UINT8(127,  pos.beatFrac8);
}

void test_beat_info_position_16th_note_subdivisions() {
    // phaseMs=100, subdivisionsPerBeat=4 → subDiv = 100/(500/4) = 100/125 = 0
    BarBeat pos = make(kInterval, 100, 0).position(4, 4);
    TEST_ASSERT_EQUAL_UINT8(0, pos.subdivision);

    // phaseMs=200 → 200/125 = 1
    BarBeat pos2 = make(kInterval, 200, 0).position(4, 4);
    TEST_ASSERT_EQUAL_UINT8(1, pos2.subdivision);
}

// ---------------------------------------------------------------------------

void beat_info_tests() {
    // Inactive
    RUN_TEST(test_beat_info_inactive_is_not_active);
    RUN_TEST(test_beat_info_inactive_returns_start_for_all_envelopes);
    RUN_TEST(test_beat_info_inactive_position_is_zero);

    // isActive
    RUN_TEST(test_beat_info_active_when_interval_nonzero);

    // tempo
    RUN_TEST(test_beat_info_tempo_inactive_is_zero);
    RUN_TEST(test_beat_info_tempo_120bpm);
    RUN_TEST(test_beat_info_tempo_60bpm);
    RUN_TEST(test_beat_info_tempo_140bpm);

    // isOnBeat
    RUN_TEST(test_beat_info_is_on_beat_at_phase_zero);
    RUN_TEST(test_beat_info_is_on_beat_within_window);
    RUN_TEST(test_beat_info_not_on_beat_at_window_boundary);
    RUN_TEST(test_beat_info_not_on_beat_past_window);

    // beatFrac8
    RUN_TEST(test_beat_info_frac8_at_onset);
    RUN_TEST(test_beat_info_frac8_at_midpoint);
    RUN_TEST(test_beat_info_frac8_near_end);

    // beatSawTime
    RUN_TEST(test_beat_saw_time_start_at_onset);
    RUN_TEST(test_beat_saw_time_midpoint_decay);
    RUN_TEST(test_beat_saw_time_end_at_boundary);
    RUN_TEST(test_beat_saw_time_holds_end_past_window);
    RUN_TEST(test_beat_saw_time_attack_direction);
    RUN_TEST(test_beat_saw_time_custom_range);

    // beatSaw
    RUN_TEST(test_beat_saw_start_at_onset);
    RUN_TEST(test_beat_saw_full_beat_midpoint);
    RUN_TEST(test_beat_saw_holds_end_after_duty_window);
    RUN_TEST(test_beat_saw_falling_direction);
    RUN_TEST(test_beat_saw_half_duty_midpoint);
    RUN_TEST(test_beat_saw_phase_shifts_onset);
    RUN_TEST(test_beat_saw_phase_wraps);

    // beatTriangle
    RUN_TEST(test_beat_triangle_start_at_onset);
    RUN_TEST(test_beat_triangle_peaks_at_midbeat);
    RUN_TEST(test_beat_triangle_start_at_end);
    RUN_TEST(test_beat_triangle_half_duty_holds_start_past_window);
    RUN_TEST(test_beat_triangle_phase_shifts_peak);

    // beatSin
    RUN_TEST(test_beat_sin_lo_at_onset);
    RUN_TEST(test_beat_sin_near_peak_at_midbeat);
    RUN_TEST(test_beat_sin_near_lo_at_end);

    // beatSquare
    RUN_TEST(test_beat_square_hi_in_duty_window);
    RUN_TEST(test_beat_square_lo_past_duty);
    RUN_TEST(test_beat_square_lo_hi_range);

    // beatExpDecay
    RUN_TEST(test_beat_exp_decay_hi_at_onset);
    RUN_TEST(test_beat_exp_decay_below_linear_at_midpoint);
    RUN_TEST(test_beat_exp_decay_near_lo_at_end);

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

    // isOnPhrase / isLastBeatOfPhrase / phraseFrac8
    RUN_TEST(test_beat_info_is_on_phrase_at_start);
    RUN_TEST(test_beat_info_is_on_phrase_at_boundary);
    RUN_TEST(test_beat_info_not_on_phrase_mid_phrase);
    RUN_TEST(test_beat_info_not_on_phrase_outside_window);
    RUN_TEST(test_beat_info_is_last_beat_of_phrase);
    RUN_TEST(test_beat_info_not_last_beat_of_phrase);
    RUN_TEST(test_beat_info_phrase_frac8_at_start);
    RUN_TEST(test_beat_info_phrase_frac8_at_midpoint);
    RUN_TEST(test_beat_info_phrase_frac8_resets_at_boundary);

    // phraseFrac16
    RUN_TEST(test_beat_info_phrase_frac16_inactive);
    RUN_TEST(test_beat_info_phrase_frac16_at_start);
    RUN_TEST(test_beat_info_phrase_frac16_at_midpoint);
    RUN_TEST(test_beat_info_phrase_frac16_resets_at_boundary);
    RUN_TEST(test_beat_info_phrase_frac16_higher_precision_than_frac8);

    // phraseView
    RUN_TEST(test_beat_info_phrase_view_inactive_returns_inactive);
    RUN_TEST(test_beat_info_phrase_view_has_virtual_interval);
    RUN_TEST(test_beat_info_phrase_view_phase_zero_at_phrase_start);
    RUN_TEST(test_beat_info_phrase_view_phase_at_midpoint);
    RUN_TEST(test_beat_info_phrase_view_beatnumber_is_phrase_count);
    RUN_TEST(test_beat_info_phrase_view_saw_rising);
    RUN_TEST(test_beat_info_phrase_view_triangle_peaks_at_mid);
    RUN_TEST(test_beat_info_phrase_view_sin_peaks_at_mid);

    // position() / BarBeat
    RUN_TEST(test_beat_info_position_at_bar_downbeat);
    RUN_TEST(test_beat_info_position_mid_phrase);
    RUN_TEST(test_beat_info_position_16th_note_subdivisions);
}
