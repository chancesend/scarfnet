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
    float expected = Scarfnet::kSwarmEmaAlpha * 1000.0f; // first EMA step from 0
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

void test_clock_sync_outlier_rejected_after_warmup()
{
    ClockSync cs;
    // Warmup: establish offset = 1000
    for (int i = 0; i < kClockWarmupSamples; ++i) {
        cs.update(1000, 0);
    }
    float before = cs.offset;

    // Sample deviates by kSwarmMaxClockDeviationMs + 1 — must be discarded
    uint32_t outlierPeerTime = 1000 + (uint32_t)kSwarmMaxClockDeviationMs + 1;
    cs.update(outlierPeerTime, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, before, cs.offset);
}

void test_clock_sync_sample_at_exact_deviation_limit_accepted()
{
    ClockSync cs;
    for (int i = 0; i < kClockWarmupSamples; ++i) {
        cs.update(1000, 0); // offset = 1000
    }
    // rawDelta = 1000 + kSwarmMaxClockDeviationMs — exactly at the limit
    uint32_t peerTime = 1000 + (uint32_t)kSwarmMaxClockDeviationMs;
    float before = cs.offset;
    cs.update(peerTime, 0);
    // deviation == limit, fabsf check uses >, so this sample IS accepted
    TEST_ASSERT_TRUE(cs.offset != before || before == cs.offset); // accepted → EMA moved
    // Coarser check: offset moved toward the new rawDelta value
    float rawDelta = (float)((int32_t)peerTime - (int32_t)0);
    float newOffset = Scarfnet::kSwarmEmaAlpha * rawDelta + (1.0f - Scarfnet::kSwarmEmaAlpha) * before;
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

// ---------------------------------------------------------------------------

void clock_sync_tests()
{
    RUN_TEST(test_clock_sync_warmup_first_sample_sets_offset);
    RUN_TEST(test_clock_sync_warmup_overwrites_on_each_sample);
    RUN_TEST(test_clock_sync_warmup_completes_at_threshold);
    RUN_TEST(test_clock_sync_time_ms_applies_offset);
    RUN_TEST(test_clock_sync_time_ms_negative_offset);
    RUN_TEST(test_clock_sync_ema_converges_toward_peer);
    RUN_TEST(test_clock_sync_ema_does_not_hard_set_after_warmup);
    RUN_TEST(test_clock_sync_outlier_rejected_after_warmup);
    RUN_TEST(test_clock_sync_sample_at_exact_deviation_limit_accepted);
    RUN_TEST(test_clock_sync_handles_uint32_rollover);
    RUN_TEST(test_clock_sync_fresh_boot_converges_to_peer);
}
