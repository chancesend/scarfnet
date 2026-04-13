#include <unity.h>
#include <mesh_time.h>

// ---------------------------------------------------------------------------
// Scarfnet::computeNodeTimeMs  (see include/mesh_time.h)
//
// Raw painlessMesh time is in microseconds (uint32_t). The function right-
// shifts by 10 (÷1024 ≈ ms), detects uint32_t rollovers, and packs the
// result as:
//   bits  0-21: lower 22 bits of nodeTimeMs
//   bits 22-31: rolloverCount
// ---------------------------------------------------------------------------

void test_computeNodeTimeMs_basic_conversion()
{
    // 1024 raw units >> 10 == 1 ms
    int32_t last = 0;
    int32_t rollovers = 0;
    uint32_t result = Scarfnet::computeNodeTimeMs(1024, last, rollovers);
    TEST_ASSERT_EQUAL_UINT32(1, result);
    TEST_ASSERT_EQUAL_INT32(0, rollovers);
    TEST_ASSERT_EQUAL_INT32(1, last);
}

void test_computeNodeTimeMs_no_rollover_on_normal_advance()
{
    // Advance from 1000 ms to 2000 ms — no rollover expected
    int32_t last = 1000;
    int32_t rollovers = 0;
    Scarfnet::computeNodeTimeMs(2000u * 1024u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(0, rollovers);
}

void test_computeNodeTimeMs_detects_rollover()
{
    // Simulate the raw uint32_t wrapping: last recorded time was near the
    // max (~4 million ms after shift), new raw reading starts near zero.
    int32_t last = 4000000; // ~4M ms, near the 22-bit ceiling
    int32_t rollovers = 0;
    Scarfnet::computeNodeTimeMs(0u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(1, rollovers);
}

void test_computeNodeTimeMs_no_spurious_rollover_on_small_drop()
{
    // A drop of less than the 1,000,000 ms threshold must not count as rollover
    int32_t last = 500000;
    int32_t rollovers = 0;
    Scarfnet::computeNodeTimeMs(1u * 1024u, last, rollovers); // drops to 1 ms
    // 1 - 500000 = -499999, which is > -1000000, so no rollover
    TEST_ASSERT_EQUAL_INT32(0, rollovers);
}

void test_computeNodeTimeMs_rollover_packed_in_upper_bits()
{
    // After one rollover the upper 10 bits of the result should be 1
    int32_t last = 4000000;
    int32_t rollovers = 0;
    uint32_t result = Scarfnet::computeNodeTimeMs(0u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(1, rollovers);
    // Upper 10 bits (bits 22-31) should equal rolloverCount (1)
    uint32_t upperBits = (result >> 22) & 0x3FFu;
    TEST_ASSERT_EQUAL_UINT32(1, upperBits);
}

void test_computeNodeTimeMs_updates_last_time()
{
    int32_t last = 0;
    int32_t rollovers = 0;
    Scarfnet::computeNodeTimeMs(5000u * 1024u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(5000, last);
}

// ---------------------------------------------------------------------------
// Long-running scenarios: multiple rollovers
// ---------------------------------------------------------------------------

void test_computeNodeTimeMs_multiple_rollovers()
{
    // Simulate three consecutive uint32_t wraps — roughly 3 × 4096 seconds
    // (~3.4 hours each at 1 ms ≈ 1024 raw units) of uptime.
    int32_t last = 0;
    int32_t rollovers = 0;

    // Advance near the ceiling
    Scarfnet::computeNodeTimeMs(4000000u * 1024u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(0, rollovers);

    // First wrap
    Scarfnet::computeNodeTimeMs(0u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(1, rollovers);

    // Advance again near ceiling
    Scarfnet::computeNodeTimeMs(4000000u * 1024u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(1, rollovers);

    // Second wrap
    Scarfnet::computeNodeTimeMs(0u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(2, rollovers);

    // Third wrap
    Scarfnet::computeNodeTimeMs(4000000u * 1024u, last, rollovers);
    Scarfnet::computeNodeTimeMs(0u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(3, rollovers);
}

void test_computeNodeTimeMs_rollover_then_advance()
{
    // After a rollover, time should continue advancing correctly from zero.
    int32_t last = 4000000;
    int32_t rollovers = 0;

    // Trigger rollover
    Scarfnet::computeNodeTimeMs(0u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(1, rollovers);

    // Advance 1000 ms post-rollover — should not trigger another rollover
    uint32_t result = Scarfnet::computeNodeTimeMs(1000u * 1024u, last, rollovers);
    TEST_ASSERT_EQUAL_INT32(1, rollovers);
    // Lower 22 bits should reflect 1000 ms
    TEST_ASSERT_EQUAL_UINT32(1000u, result & 0x003fffffu);
}

void test_computeNodeTimeMs_upper_bits_track_rollover_count()
{
    // After N rollovers, the upper 10 bits of the result must equal N.
    int32_t last = 0;
    int32_t rollovers = 0;

    for (int expected = 1; expected <= 5; expected++)
    {
        last = 4000000; // set last high so the next call at 0 triggers rollover
        uint32_t result = Scarfnet::computeNodeTimeMs(0u, last, rollovers);
        TEST_ASSERT_EQUAL_INT32(expected, rollovers);
        uint32_t upperBits = (result >> 22) & 0x3FFu;
        TEST_ASSERT_EQUAL_UINT32((uint32_t)expected, upperBits);
    }
}

void mesh_tests()
{
    RUN_TEST(test_computeNodeTimeMs_basic_conversion);
    RUN_TEST(test_computeNodeTimeMs_no_rollover_on_normal_advance);
    RUN_TEST(test_computeNodeTimeMs_detects_rollover);
    RUN_TEST(test_computeNodeTimeMs_no_spurious_rollover_on_small_drop);
    RUN_TEST(test_computeNodeTimeMs_rollover_packed_in_upper_bits);
    RUN_TEST(test_computeNodeTimeMs_updates_last_time);
    RUN_TEST(test_computeNodeTimeMs_multiple_rollovers);
    RUN_TEST(test_computeNodeTimeMs_rollover_then_advance);
    RUN_TEST(test_computeNodeTimeMs_upper_bits_track_rollover_count);
}
