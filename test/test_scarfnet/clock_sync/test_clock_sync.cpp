#include <unity.h>
#include <clock_sync.h>

using Scarfnet::ClockSync;

// ---------------------------------------------------------------------------
// Warmup phase: hard-set for first kClockWarmupSamples samples
// ---------------------------------------------------------------------------

void test_clock_sync_warmup_first_sample_sets_offset()
{
    ClockSync cs;
    // Fresh boot: local millis()≈0, peer is 60 000ms ahead.
    cs.update(60000, 0);
    TEST_ASSERT_EQUAL_INT32(60000, (int32_t)cs.offset);
    TEST_ASSERT_EQUAL_INT(1, cs.samples);
    TEST_ASSERT_FALSE(cs.isWarmedUp());
}

void test_clock_sync_warmup_overwrites_on_each_sample()
{
    ClockSync cs;
    cs.update(60000, 0);  // offset = 60000
    cs.update(61000, 1000); // rawDelta = 60000 again — but let's use a different peer time
    // rawDelta = 61000 - 1000 = 60000; offset stays 60000
    TEST_ASSERT_EQUAL_INT32(60000, (int32_t)cs.offset);
    TEST_ASSERT_EQUAL_INT(2, cs.samples);
}

void test_clock_sync_warmup_completes_at_threshold()
{
    ClockSync cs;
    for (int i = 0; i < kClockWarmupSamples; ++i) {
        cs.update(1000 * (uint32_t)(i + 1), 0);
    }
    TEST_ASSERT_TRUE(cs.isWarmedUp());
    TEST_ASSERT_EQUAL_INT(kClockWarmupSamples, cs.samples);
}

// ---------------------------------------------------------------------------
// timeMs: offset applied correctly
// ---------------------------------------------------------------------------

void test_clock_sync_time_ms_applies_offset()
{
    ClockSync cs;
    cs.update(5000, 0); // offset = 5000, local=0
    // timeMs(1000) = 1000 + 5000 = 6000
    TEST_ASSERT_EQUAL_UINT32(6000, cs.timeMs(1000));
}

void test_clock_sync_time_ms_negative_offset()
{
    ClockSync cs;
    // Peer is behind us: peer=1000, local=3000 → rawDelta = -2000
    cs.update(1000, 3000);
    TEST_ASSERT_EQUAL_INT32(-2000, (int32_t)cs.offset);
    // timeMs(3000) = 3000 + (-2000) = 1000
    TEST_ASSERT_EQUAL_UINT32(1000, cs.timeMs(3000));
}

// ---------------------------------------------------------------------------
// EMA convergence after warmup
// ---------------------------------------------------------------------------

void test_clock_sync_ema_converges_toward_peer()
{
    ClockSync cs;
    // Burn through warmup with offset = 0 (peer and local in sync)
    for (int i = 0; i < kClockWarmupSamples; ++i) {
        cs.update(100, 100); // rawDelta = 0
    }
    TEST_ASSERT_TRUE(cs.isWarmedUp());
    TEST_ASSERT_EQUAL_INT32(0, (int32_t)cs.offset);

    // Now peer drifts 1000ms ahead; EMA should move offset toward +1000
    cs.update(2000, 1000); // rawDelta = 1000; deviation = 1000 < 5000 → accepted
    float expected = kClockEmaAlpha * 1000.0f; // first EMA step from 0
    // Allow 1ms float rounding tolerance
    TEST_ASSERT_FLOAT_WITHIN(1.0f, expected, cs.offset);
}

void test_clock_sync_ema_does_not_hard_set_after_warmup()
{
    ClockSync cs;
    // Warmup with large offset
    for (int i = 0; i < kClockWarmupSamples; ++i) {
        cs.update(50000, 0); // offset hard-set to 50000
    }
    float before = cs.offset;

    // One more sample within deviation: EMA step, NOT hard-set
    cs.update(50100, 100); // rawDelta = 50000; same as before
    float after = cs.offset;
    // EMA step from 50000 toward 50000 stays at 50000 — offset unchanged
    TEST_ASSERT_FLOAT_WITHIN(1.0f, before, after);
    // samples counter should not increment beyond kClockWarmupSamples
    TEST_ASSERT_EQUAL_INT(kClockWarmupSamples, cs.samples);
}

// ---------------------------------------------------------------------------
// Outlier rejection after warmup
// ---------------------------------------------------------------------------

void test_clock_sync_far_behind_sample_ignored_after_warmup()
{
    ClockSync cs;
    // Warmup: establish offset = 10000
    for (int i = 0; i < kClockWarmupSamples; ++i) {
        cs.update(10000, 0);
    }
    float before = cs.offset;

    // Sample behind by more than kClockMaxDeviationMs — ignored
    uint32_t behindPeerTime = 10000 - (uint32_t)kClockMaxDeviationMs - 1;
    cs.update(behindPeerTime, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, before, cs.offset);
    TEST_ASSERT_TRUE(cs.isWarmedUp());
}

void test_clock_sync_far_ahead_sample_jumps_to_it()
{
    ClockSync cs;
    for (int i = 0; i < kClockWarmupSamples; ++i) {
        cs.update(1000, 0);  // offset = 1000
    }
    // Sample ahead by more than the limit: hard-set to it, stay warmed up
    uint32_t aheadPeerTime = 1000 + (uint32_t)kClockMaxDeviationMs + 1;
    cs.update(aheadPeerTime, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, (float)aheadPeerTime, cs.offset);
    TEST_ASSERT_TRUE(cs.isWarmedUp());
}

void test_clock_sync_after_jump_behind_domain_cannot_drag_back()
{
    // Just jumped to the ahead domain; next heartbeat is from a behind-domain
    // peer that hasn't jumped yet. It must be ignored, not re-adopted.
    ClockSync cs;
    for (int i = 0; i < kClockWarmupSamples; ++i) cs.update(1000, 0);
    cs.update(1000000, 0);  // jump ahead
    cs.update(1000, 0);     // old domain
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1000000.0f, cs.offset);
}

// The field split: two groups warmed up on each other ~4879 s apart. Every
// scarf must end up on the furthest-ahead clock, and stay there.
void test_clock_sync_two_warmed_domains_merge_on_ahead_clock()
{
    const uint32_t kGap = 4879000u;
    ClockSync behind, ahead;
    // Each local millis() is 100000; behind group's network time 558000,
    // ahead group's network time 558000 + kGap.
    for (int i = 0; i < kClockWarmupSamples; ++i) {
        behind.update(558000u, 100000u);
        ahead.update(558000u + kGap, 100000u);
    }

    // Exchange heartbeats for a few rounds (5 s apart).
    for (uint32_t r = 1; r <= 4; ++r) {
        uint32_t local = 100000u + r * 5000u;
        uint32_t tBehind = behind.timeMs(local);
        uint32_t tAhead  = ahead.timeMs(local);
        behind.update(tAhead, local);
        ahead.update(tBehind, local);
    }

    uint32_t local = 100000u + 5u * 5000u;
    int32_t gap = (int32_t)(ahead.timeMs(local) - behind.timeMs(local));
    TEST_ASSERT_INT32_WITHIN(5, 0, gap);
    TEST_ASSERT_EQUAL_UINT32(558000u + kGap + 5u * 5000u, ahead.timeMs(local));  // ahead never moved
    TEST_ASSERT_TRUE(behind.isWarmedUp());
}

void test_clock_sync_update_reports_action()
{
    auto warmedUp = []() {
        ClockSync cs;
        for (int i = 0; i < kClockWarmupSamples; ++i) cs.update(10000, 0);
        return cs;
    };
    ClockSync fresh;
    TEST_ASSERT_TRUE(fresh.update(1000, 0) == ClockSync::Action::Warmup);
    TEST_ASSERT_TRUE(warmedUp().update(10100, 0) == ClockSync::Action::Track);
    TEST_ASSERT_TRUE(warmedUp().update(10000 + kClockMaxDeviationMs + 1, 0)
                     == ClockSync::Action::JumpAhead);
    TEST_ASSERT_TRUE(warmedUp().update(10000 - kClockMaxDeviationMs - 1, 0)
                     == ClockSync::Action::IgnoreBehind);
}

void test_clock_sync_sample_at_exact_deviation_limit_accepted()
{
    ClockSync cs;
    for (int i = 0; i < kClockWarmupSamples; ++i) {
        cs.update(1000, 0); // offset = 1000
    }
    // rawDelta = 1000 + kClockMaxDeviationMs — exactly at the limit
    uint32_t peerTime = 1000 + (uint32_t)kClockMaxDeviationMs;
    float before = cs.offset;
    cs.update(peerTime, 0);
    // deviation == limit is still in range, so the sample is tracked
    float rawDelta = (float)((int32_t)peerTime - (int32_t)0);
    float newOffset = kClockEmaAlpha * rawDelta + (1.0f - kClockEmaAlpha) * before;
    TEST_ASSERT_FLOAT_WITHIN(1.0f, newOffset, cs.offset);
}

// ---------------------------------------------------------------------------
// Sign / rollover handling
// ---------------------------------------------------------------------------

void test_clock_sync_handles_uint32_rollover()
{
    ClockSync cs;
    // Peer time near uint32_t max, local millis() near zero.
    // int32_t subtraction wraps correctly: (int32_t)0xFFFFF000 - (int32_t)0x00000100
    // = negative (large peer - small local interpreted as large signed negative).
    // This is intentional: the offset will be large-negative, tracking the "behind" peer.
    uint32_t peerTime = 0xFFFFF000u;
    uint32_t localMs  = 0x00000100u;
    cs.update(peerTime, localMs);
    // Just verify no crash and that timeMs() round-trips
    uint32_t synced = cs.timeMs(localMs);
    // synced should equal peerTime (hard-set during warmup)
    TEST_ASSERT_EQUAL_UINT32(peerTime, synced);
}

void test_clock_sync_fresh_boot_converges_to_peer()
{
    // Simulate the canonical fresh-boot scenario:
    // - Our millis() is near 0 (just booted)
    // - Established peers are at hours-long timestamps (~3.6M ms = 1 hour)
    ClockSync cs;
    uint32_t peerMs = 3600000u; // 1 hour uptime
    for (int i = 0; i < kClockWarmupSamples; ++i) {
        cs.update(peerMs, (uint32_t)i * 5000u); // local advances by 5s per heartbeat
    }
    // After warmup, timeMs(localMs) should be close to peer's time
    uint32_t localNow = (uint32_t)kClockWarmupSamples * 5000u;
    uint32_t synced = cs.timeMs(localNow);
    // Expect within 15s (3 × 5s heartbeat drift)
    int32_t diff = (int32_t)synced - (int32_t)peerMs;
    TEST_ASSERT_TRUE(diff >= -15000 && diff <= 15000);
}

// Days of network uptime: offsets far past float's 1 ms resolution (2^24 ms)
// must still track millisecond corrections.
void test_clock_sync_tracks_small_corrections_at_large_offset()
{
    const uint32_t kNetworkMs = 260000000u;  // ~3 days
    ClockSync cs;
    for (int i = 0; i < kClockWarmupSamples; ++i) cs.update(kNetworkMs, 1000);
    const int32_t base = (int32_t)(kNetworkMs - 1000u);
    TEST_ASSERT_EQUAL_INT32(base, (int32_t)cs.offset);

    for (int i = 0; i < 20; ++i) cs.update(kNetworkMs + 3u, 1000);  // peer now 3 ms ahead
    TEST_ASSERT_INT32_WITHIN(1, base + 3, (int32_t)cs.offset);
    TEST_ASSERT_INT32_WITHIN(1, (int32_t)(kNetworkMs + 3u + 60000u), (int32_t)cs.timeMs(61000));
}

void test_clock_sync_tracks_across_uint32_wrap()
{
    ClockSync cs;
    for (int i = 0; i < kClockWarmupSamples; ++i) cs.update(0xFFFFFF00u, 1000);
    cs.update(0xFFFFFF00u + 10u, 1000);  // 10 ms ahead → target moves 4 ms
    // 0x200 ms later the network clock has wrapped past zero.
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFF00u + 0x200u + 4u, cs.timeMs(1000 + 0x200));
    TEST_ASSERT_TRUE(cs.update(0x100u, 1000 + 0x200) == ClockSync::Action::Track);
}

// ---------------------------------------------------------------------------
// Slewing: small corrections play out gradually; hard-sets apply at once
// ---------------------------------------------------------------------------

static void warmAtZero(ClockSync& cs)
{
    for (int i = 0; i < kClockWarmupSamples; ++i) cs.update(0, 0);  // offset 0 at local 0
}

void test_clock_sync_small_correction_slews_not_skips()
{
    ClockSync cs;
    warmAtZero(cs);
    cs.update(11000, 10000);  // peer 1000 ms ahead → target 400
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 400.0f, cs.offset);

    // Right after the sample the displayed clock hasn't moved...
    TEST_ASSERT_EQUAL_UINT32(10000, cs.timeMs(10000));
    // ...then gains kClockSlewRate ms per ms until it reaches the target.
    TEST_ASSERT_EQUAL_UINT32(10100 + (uint32_t)(100 * kClockSlewRate), cs.timeMs(10100));
    uint32_t reachedAt = 10000 + (uint32_t)(400 / kClockSlewRate);
    TEST_ASSERT_EQUAL_UINT32(reachedAt + 400, cs.timeMs(reachedAt));
    TEST_ASSERT_EQUAL_UINT32(reachedAt + 1000 + 400, cs.timeMs(reachedAt + 1000));
}

void test_clock_sync_negative_correction_never_runs_backward()
{
    ClockSync cs;
    warmAtZero(cs);
    uint32_t prev = cs.timeMs(9990);  // just before the correction arrives
    cs.update(10000 - 4000, 10000);  // peer 4000 ms behind (within limit) → target -1600
    TEST_ASSERT_TRUE_MESSAGE(cs.timeMs(10000) >= prev, "synced clock ran backward");
    prev = cs.timeMs(10000);
    for (uint32_t t = 10010; t < 20000; t += 10) {
        uint32_t now = cs.timeMs(t);
        TEST_ASSERT_TRUE_MESSAGE(now >= prev, "synced clock ran backward");
        prev = now;
    }
    TEST_ASSERT_EQUAL_UINT32(20000 - 1600, cs.timeMs(20000));
}

void test_clock_sync_max_slewed_gap_closes_within_10s()
{
    // Worst case: displayed offset 5 s from target. Build it by hard-setting
    // the slew start, then moving the target with in-range samples.
    ClockSync cs;
    warmAtZero(cs);
    for (int i = 0; i < 20; ++i) cs.update(5000, 0);  // same instant: target → ~5000, display 0
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 5000.0f, cs.offset);
    TEST_ASSERT_EQUAL_UINT32(0, cs.timeMs(0));
    uint32_t closesBy = (uint32_t)(5000 / kClockSlewRate);
    TEST_ASSERT_TRUE(closesBy <= 10000);
    TEST_ASSERT_INT32_WITHIN(5, (int32_t)(closesBy + 5000), (int32_t)cs.timeMs(closesBy));
}

void test_clock_sync_new_sample_mid_slew_continues_smoothly()
{
    ClockSync cs;
    warmAtZero(cs);
    cs.update(11000, 10000);           // target 400
    uint32_t before = cs.timeMs(10200);  // mid-slew: +100
    cs.update(11200, 10200);           // another sample mid-slew
    uint32_t after = cs.timeMs(10200);
    TEST_ASSERT_EQUAL_UINT32(before, after);  // no skip at the instant of the update
}

void test_clock_sync_jump_ahead_is_immediate()
{
    ClockSync cs;
    warmAtZero(cs);
    cs.update(1000000, 10000);  // far ahead → hard jump
    TEST_ASSERT_EQUAL_UINT32(1000000, cs.timeMs(10000));
}

// ---------------------------------------------------------------------------

void clock_sync_tests()
{
    RUN_TEST(test_clock_sync_small_correction_slews_not_skips);
    RUN_TEST(test_clock_sync_negative_correction_never_runs_backward);
    RUN_TEST(test_clock_sync_max_slewed_gap_closes_within_10s);
    RUN_TEST(test_clock_sync_new_sample_mid_slew_continues_smoothly);
    RUN_TEST(test_clock_sync_jump_ahead_is_immediate);
    RUN_TEST(test_clock_sync_warmup_first_sample_sets_offset);
    RUN_TEST(test_clock_sync_warmup_overwrites_on_each_sample);
    RUN_TEST(test_clock_sync_warmup_completes_at_threshold);
    RUN_TEST(test_clock_sync_time_ms_applies_offset);
    RUN_TEST(test_clock_sync_time_ms_negative_offset);
    RUN_TEST(test_clock_sync_ema_converges_toward_peer);
    RUN_TEST(test_clock_sync_ema_does_not_hard_set_after_warmup);
    RUN_TEST(test_clock_sync_far_behind_sample_ignored_after_warmup);
    RUN_TEST(test_clock_sync_far_ahead_sample_jumps_to_it);
    RUN_TEST(test_clock_sync_after_jump_behind_domain_cannot_drag_back);
    RUN_TEST(test_clock_sync_two_warmed_domains_merge_on_ahead_clock);
    RUN_TEST(test_clock_sync_update_reports_action);
    RUN_TEST(test_clock_sync_sample_at_exact_deviation_limit_accepted);
    RUN_TEST(test_clock_sync_tracks_small_corrections_at_large_offset);
    RUN_TEST(test_clock_sync_tracks_across_uint32_wrap);
    RUN_TEST(test_clock_sync_handles_uint32_rollover);
    RUN_TEST(test_clock_sync_fresh_boot_converges_to_peer);
}
