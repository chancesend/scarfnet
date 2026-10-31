#include <unity.h>
#include <tempo_sync.h>

using Scarfnet::TempoSync;
using Scarfnet::TimeMs;

// Deliver `from`'s heartbeat (as sent at `now`) to `to`.
static TempoSync::Result deliver(const TempoSync& from, TempoSync& to, TimeMs now)
{
    return to.receive(from.index, from.tempo.beatIntervalMs(), now, from.tempo.beatPhaseMs(now));
}

// Tap twice to set a tempo of `intervalMs`, finishing at `start + intervalMs`.
static void tapTempo(TempoSync& s, TimeMs start, uint16_t intervalMs)
{
    s.tap(start);
    s.tap(start + intervalMs);
}

// ─── Index ordering ──────────────────────────────────────────────────────────

void test_tempo_index_newer_handles_wraparound()
{
    TEST_ASSERT_TRUE(Scarfnet::tempoIndexNewer(6, 5));
    TEST_ASSERT_FALSE(Scarfnet::tempoIndexNewer(5, 6));
    TEST_ASSERT_FALSE(Scarfnet::tempoIndexNewer(5, 5));
    TEST_ASSERT_TRUE(Scarfnet::tempoIndexNewer(0, 65535));
    TEST_ASSERT_FALSE(Scarfnet::tempoIndexNewer(65535, 0));
}

// ─── Local events ────────────────────────────────────────────────────────────

void test_tempo_enter_tap_mode_bumps_index_and_keeps_tempo()
{
    TempoSync s;
    s.tempo.setFromPacket(500, 1000, 0);
    s.enterTapMode();
    TEST_ASSERT_TRUE(s.tapMode);
    TEST_ASSERT_EQUAL_UINT16(1, s.index);
    TEST_ASSERT_EQUAL_UINT16(500, s.tempo.beatIntervalMs());
}

void test_tempo_first_tap_without_tempo_is_not_announced()
{
    // The first tap of a fresh sequence has no tempo yet; announcing it would
    // clear the network's tempo until the second tap.
    TempoSync s;
    s.enterTapMode();
    uint16_t afterEnter = s.index;
    s.tap(1000);
    TEST_ASSERT_EQUAL_UINT16(afterEnter, s.index);
    s.tap(1600);
    TEST_ASSERT_EQUAL_UINT16(afterEnter + 1, s.index);
    TEST_ASSERT_EQUAL_UINT16(600, s.tempo.beatIntervalMs());
}

void test_tempo_exit_tap_mode_clears_tempo_and_bumps_index()
{
    TempoSync s;
    s.enterTapMode();
    tapTempo(s, 1000, 600);
    uint16_t before = s.index;
    s.exitTapMode();
    TEST_ASSERT_FALSE(s.tapMode);
    TEST_ASSERT_FALSE(s.tempo.isActive());
    TEST_ASSERT_EQUAL_UINT16(before + 1, s.index);
}

// ─── Followers ───────────────────────────────────────────────────────────────

void test_tempo_follower_adopts_newer_tempo()
{
    TempoSync master, follower;
    master.enterTapMode();
    tapTempo(master, 1000, 600);
    TEST_ASSERT_TRUE(deliver(master, follower, 2000) == TempoSync::Result::Adopted);
    TEST_ASSERT_EQUAL_UINT16(600, follower.tempo.beatIntervalMs());
    TEST_ASSERT_EQUAL_UINT16(master.index, follower.index);
}

void test_tempo_stale_no_tempo_heartbeat_cannot_clear_tempo()
{
    // The v111 field bug: a heartbeat with beat=0 cleared every follower.
    TempoSync follower;
    follower.receive(/*index=*/10, 600, 1000, 0);
    TEST_ASSERT_TRUE(follower.receive(/*index=*/9, 0, 2000, 0) == TempoSync::Result::Ignored);
    TEST_ASSERT_TRUE(follower.receive(/*index=*/10, 0, 2000, 0) == TempoSync::Result::Ignored);
    TEST_ASSERT_EQUAL_UINT16(600, follower.tempo.beatIntervalMs());
}

void test_tempo_relayed_duplicate_is_ignored()
{
    TempoSync a, b;
    a.enterTapMode();
    tapTempo(a, 1000, 600);
    deliver(a, b, 2000);
    TEST_ASSERT_TRUE(deliver(b, a, 2100) == TempoSync::Result::Ignored);  // relay back to master
    TEST_ASSERT_TRUE(a.tapMode);
}

void test_tempo_master_exit_clears_followers()
{
    TempoSync master, follower;
    master.enterTapMode();
    tapTempo(master, 1000, 600);
    deliver(master, follower, 2000);
    master.exitTapMode();
    TEST_ASSERT_TRUE(deliver(master, follower, 3000) == TempoSync::Result::Adopted);
    TEST_ASSERT_FALSE(follower.tempo.isActive());
}

// ─── Only one scarf in tap mode ──────────────────────────────────────────────

void test_tempo_second_scarf_entering_takes_over()
{
    TempoSync a, b;
    a.enterTapMode();
    tapTempo(a, 1000, 600);
    deliver(a, b, 2000);

    b.enterTapMode();  // B takes over, carrying A's tempo
    TEST_ASSERT_TRUE(deliver(b, a, 2500) == TempoSync::Result::TakenOver);
    TEST_ASSERT_FALSE(a.tapMode);
    TEST_ASSERT_TRUE(b.tapMode);
    TEST_ASSERT_EQUAL_UINT16(600, a.tempo.beatIntervalMs());  // tempo kept
}

void test_tempo_simultaneous_entry_resolves_on_first_tap()
{
    TempoSync a, b;
    a.enterTapMode();  // both enter from index 0 at the same moment
    b.enterTapMode();
    TEST_ASSERT_TRUE(deliver(a, b, 1000) == TempoSync::Result::Ignored);  // same state
    TEST_ASSERT_TRUE(deliver(b, a, 1000) == TempoSync::Result::Ignored);
    TEST_ASSERT_TRUE(a.tapMode && b.tapMode);

    tapTempo(a, 2000, 700);  // A taps first
    TEST_ASSERT_TRUE(deliver(a, b, 3000) == TempoSync::Result::TakenOver);
    TEST_ASSERT_FALSE(b.tapMode);
    TEST_ASSERT_EQUAL_UINT16(700, b.tempo.beatIntervalMs());
}

void test_tempo_simultaneous_taps_converge_on_higher_interval()
{
    // Both masters set a tempo at the same index; followers hear them in
    // opposite orders. Everyone must end on the same tempo.
    TempoSync a, b, c, d;
    a.enterTapMode();
    b.enterTapMode();
    tapTempo(a, 1000, 500);
    tapTempo(b, 1000, 800);
    TEST_ASSERT_EQUAL_UINT16(a.index, b.index);

    deliver(a, c, 2000); deliver(b, c, 2000);
    deliver(b, d, 2000); deliver(a, d, 2000);
    TEST_ASSERT_TRUE(deliver(b, a, 2000) == TempoSync::Result::TakenOver);  // 800 wins the tie
    TEST_ASSERT_TRUE(deliver(a, b, 2000) == TempoSync::Result::Ignored);

    TempoSync* all[] = {&a, &b, &c, &d};
    for (auto* s : all) TEST_ASSERT_EQUAL_UINT16(800, s->tempo.beatIntervalMs());
    TEST_ASSERT_FALSE(a.tapMode);
    TEST_ASSERT_TRUE(b.tapMode);
}

void tempo_sync_tests()
{
    RUN_TEST(test_tempo_index_newer_handles_wraparound);
    RUN_TEST(test_tempo_enter_tap_mode_bumps_index_and_keeps_tempo);
    RUN_TEST(test_tempo_first_tap_without_tempo_is_not_announced);
    RUN_TEST(test_tempo_exit_tap_mode_clears_tempo_and_bumps_index);
    RUN_TEST(test_tempo_follower_adopts_newer_tempo);
    RUN_TEST(test_tempo_stale_no_tempo_heartbeat_cannot_clear_tempo);
    RUN_TEST(test_tempo_relayed_duplicate_is_ignored);
    RUN_TEST(test_tempo_master_exit_clears_followers);
    RUN_TEST(test_tempo_second_scarf_entering_takes_over);
    RUN_TEST(test_tempo_simultaneous_entry_resolves_on_first_tap);
    RUN_TEST(test_tempo_simultaneous_taps_converge_on_higher_interval);
}
