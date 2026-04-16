#include <unity.h>
#include <swarm_ema.h>

// ---------------------------------------------------------------------------
// swarmDeltaIsPlausible — clamp logic
// ---------------------------------------------------------------------------

void test_swarm_plausible_small_positive()
{
    TEST_ASSERT_TRUE(Scarfnet::swarmDeltaIsPlausible(100, 5000));
}

void test_swarm_plausible_small_negative()
{
    TEST_ASSERT_TRUE(Scarfnet::swarmDeltaIsPlausible(-100, 5000));
}

void test_swarm_plausible_exactly_at_limit()
{
    // Boundary: == max is accepted (not strictly greater than).
    TEST_ASSERT_TRUE(Scarfnet::swarmDeltaIsPlausible(5000, 5000));
    TEST_ASSERT_TRUE(Scarfnet::swarmDeltaIsPlausible(-5000, 5000));
}

void test_swarm_plausible_just_over_limit()
{
    TEST_ASSERT_FALSE(Scarfnet::swarmDeltaIsPlausible(5001, 5000));
    TEST_ASSERT_FALSE(Scarfnet::swarmDeltaIsPlausible(-5001, 5000));
}

// Simulate the exact crash/rejoin scenario from log1: node rebooted with
// clock ≈ 301ms, mesh had been running ≈ 30 minutes → delta ≈ -1,777,754ms.
void test_swarm_post_crash_delta_discarded()
{
    TEST_ASSERT_FALSE(Scarfnet::swarmDeltaIsPlausible(-1777754, 5000));
}

// Simulate the worst case in the logs: node 142656849, initial smoothed value
// implied a seed near -16,000,000ms (several hours of clock skew).
void test_swarm_extreme_clock_skew_discarded()
{
    TEST_ASSERT_FALSE(Scarfnet::swarmDeltaIsPlausible(-16000000, 5000));
}

// ---------------------------------------------------------------------------
// swarmEmaUpdate — EMA arithmetic
// ---------------------------------------------------------------------------

void test_swarm_ema_first_sample_from_zero_seed()
{
    // Seed = 0 (new node policy).  First good sample of 100ms should produce
    // 0.2*100 + 0.8*0 = 20ms, NOT 100ms (the old bug: seed = raw).
    int32_t result = Scarfnet::swarmEmaUpdate(100, 0);
    TEST_ASSERT_EQUAL_INT32(20, result);
}

void test_swarm_ema_converges_toward_steady_state()
{
    // Apply 100ms samples repeatedly from a 0 seed.  After enough steps the
    // smoothed value should approach 100ms.
    int32_t smoothed = 0;
    for (int i = 0; i < 40; i++)
        smoothed = Scarfnet::swarmEmaUpdate(100, smoothed);
    // After 40 steps (0.8^40 ≈ 0.013%) the residual error is negligible.
    // Float→int truncation at each step creates a small steady-state offset
    // (~4ms), so allow ±5ms tolerance rather than strict equality.
    TEST_ASSERT_INT32_WITHIN(5, 100, smoothed);
}

void test_swarm_ema_step_response()
{
    // Start settled at 0, then switch to a steady 200ms input.  Verify the
    // smoothed value is within 5% of 200ms after 30 steps (≈2.5 minutes).
    int32_t smoothed = 0;
    for (int i = 0; i < 30; i++)
        smoothed = Scarfnet::swarmEmaUpdate(200, smoothed);
    TEST_ASSERT_INT32_WITHIN(10, 200, smoothed);
}

// ---------------------------------------------------------------------------
// Combined: simulate the bug scenario end-to-end
//
// Old behaviour (no fix):
//   - Seed = first raw sample (-1,777,754ms)
//   - Recovery takes ~80 heartbeats
//
// New behaviour (A+B fix):
//   - Bad sample discarded by clamp → seed stays 0
//   - Good samples start arriving → converges in ~20 heartbeats
// ---------------------------------------------------------------------------

void test_swarm_rejoin_scenario_bad_sample_then_good_samples()
{
    const int32_t kMax = 5000;
    int32_t smoothed = 0;  // new-node seed

    // First heartbeat after rejoin: clock wildly off → discarded.
    int32_t bad_delta = -1777754;
    if (Scarfnet::swarmDeltaIsPlausible(bad_delta, kMax))
        smoothed = Scarfnet::swarmEmaUpdate(bad_delta, smoothed);
    // Smoothed must still be 0 — the bad sample was rejected.
    TEST_ASSERT_EQUAL_INT32(0, smoothed);

    // Subsequent heartbeats once the clock has settled: raw ≈ 100ms.
    for (int i = 0; i < 20; i++)
    {
        int32_t raw = 100;
        if (Scarfnet::swarmDeltaIsPlausible(raw, kMax))
            smoothed = Scarfnet::swarmEmaUpdate(raw, smoothed);
    }
    // After 20 good heartbeats (~100s) the value should be close to 100ms.
    TEST_ASSERT_INT32_WITHIN(5, 100, smoothed);
}

void test_swarm_rejoin_scenario_old_behaviour_for_reference()
{
    // Demonstrate what the OLD code did (seed = first raw sample, no clamp).
    // This is NOT a correctness test — it documents the bug so the numbers
    // in the analysis doc are reproducible.  We assert the smoothed value is
    // still far from the true ~100ms after 20 heartbeats.
    int32_t smoothed = -1777754;  // old: seed = raw
    for (int i = 0; i < 20; i++)
        smoothed = Scarfnet::swarmEmaUpdate(100, smoothed);
    // After 20 steps: 0.8^20 ≈ 1.15% of seed remains → ~-20,400ms residual.
    TEST_ASSERT_TRUE(smoothed < -10000);
}

void test_swarm_bad_sample_mid_stream_does_not_corrupt()
{
    const int32_t kMax = 5000;
    int32_t smoothed = 0;

    // Settle on ~100ms.
    for (int i = 0; i < 30; i++)
    {
        if (Scarfnet::swarmDeltaIsPlausible(100, kMax))
            smoothed = Scarfnet::swarmEmaUpdate(100, smoothed);
    }
    int32_t settled = smoothed;
    TEST_ASSERT_INT32_WITHIN(5, 100, settled);

    // Inject a bad sample (e.g. a brief clock correction spike).
    int32_t bad = -910453;
    if (Scarfnet::swarmDeltaIsPlausible(bad, kMax))
        smoothed = Scarfnet::swarmEmaUpdate(bad, smoothed);

    // Smoothed must be unchanged — the bad sample was rejected.
    TEST_ASSERT_EQUAL_INT32(settled, smoothed);
}

// ---------------------------------------------------------------------------

void swarm_ema_tests()
{
    RUN_TEST(test_swarm_plausible_small_positive);
    RUN_TEST(test_swarm_plausible_small_negative);
    RUN_TEST(test_swarm_plausible_exactly_at_limit);
    RUN_TEST(test_swarm_plausible_just_over_limit);
    RUN_TEST(test_swarm_post_crash_delta_discarded);
    RUN_TEST(test_swarm_extreme_clock_skew_discarded);
    RUN_TEST(test_swarm_ema_first_sample_from_zero_seed);
    RUN_TEST(test_swarm_ema_converges_toward_steady_state);
    RUN_TEST(test_swarm_ema_step_response);
    RUN_TEST(test_swarm_rejoin_scenario_bad_sample_then_good_samples);
    RUN_TEST(test_swarm_rejoin_scenario_old_behaviour_for_reference);
    RUN_TEST(test_swarm_bad_sample_mid_stream_does_not_corrupt);
}
