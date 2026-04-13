#include <unity.h>
#include <sync.h>

// ---------------------------------------------------------------------------
// shouldAcceptUpdate
// ---------------------------------------------------------------------------

void test_sync_accept_newer_index()
{
    TEST_ASSERT_TRUE(Scarfnet::shouldAcceptUpdate(5, 4));
}

void test_sync_reject_equal_index()
{
    // Equal changeIndex means we already have this state — ignore it.
    TEST_ASSERT_FALSE(Scarfnet::shouldAcceptUpdate(4, 4));
}

void test_sync_reject_older_index()
{
    // Older changeIndex from a lagging node or a duplicate in-flight message.
    TEST_ASSERT_FALSE(Scarfnet::shouldAcceptUpdate(3, 4));
}

void test_sync_accept_first_update_from_zero()
{
    // A freshly booted node with changeIndex 0 should accept the first broadcast.
    TEST_ASSERT_TRUE(Scarfnet::shouldAcceptUpdate(1, 0));
}

void test_sync_reject_zero_when_already_ahead()
{
    // A reset/restarted node sending changeIndex 0 must not roll back a node
    // that has already processed button presses.
    TEST_ASSERT_FALSE(Scarfnet::shouldAcceptUpdate(0, 10));
}

// ---------------------------------------------------------------------------
// rolloverGuard
// ---------------------------------------------------------------------------

void test_rollover_guard_passes_normal_value()
{
    TEST_ASSERT_EQUAL_UINT32(42u, Scarfnet::rolloverGuard(42u));
}

void test_rollover_guard_passes_at_threshold()
{
    // Exactly 0x7fffffff should pass through unchanged.
    TEST_ASSERT_EQUAL_UINT32(0x7fffffffu, Scarfnet::rolloverGuard(0x7fffffffu));
}

void test_rollover_guard_resets_above_threshold()
{
    // One above the threshold triggers a reset to 0.
    TEST_ASSERT_EQUAL_UINT32(0u, Scarfnet::rolloverGuard(0x80000000u));
}

void test_rollover_guard_resets_max_uint32()
{
    TEST_ASSERT_EQUAL_UINT32(0u, Scarfnet::rolloverGuard(0xffffffffu));
}

// ---------------------------------------------------------------------------
// Combined: simulate a sequence of out-of-order / duplicate messages
// ---------------------------------------------------------------------------

void test_sync_out_of_order_messages_do_not_regress()
{
    // Simulate a node that processes messages: 1, 3, 2 (out of order).
    // Only 1 and 3 should be accepted; 2 arrives late and must be dropped.
    uint32_t local = 0;

    TEST_ASSERT_TRUE(Scarfnet::shouldAcceptUpdate(1, local));
    local = Scarfnet::rolloverGuard(1);
    TEST_ASSERT_EQUAL_UINT32(1u, local);

    TEST_ASSERT_TRUE(Scarfnet::shouldAcceptUpdate(3, local));
    local = Scarfnet::rolloverGuard(3);
    TEST_ASSERT_EQUAL_UINT32(3u, local);

    // Late message with changeIndex 2 — must be rejected.
    TEST_ASSERT_FALSE(Scarfnet::shouldAcceptUpdate(2, local));
    TEST_ASSERT_EQUAL_UINT32(3u, local); // state unchanged
}

void test_sync_duplicate_messages_ignored()
{
    uint32_t local = 7;
    // Same message delivered twice (e.g. from two mesh paths).
    TEST_ASSERT_FALSE(Scarfnet::shouldAcceptUpdate(7, local));
    TEST_ASSERT_FALSE(Scarfnet::shouldAcceptUpdate(7, local));
}

void sync_tests()
{
    RUN_TEST(test_sync_accept_newer_index);
    RUN_TEST(test_sync_reject_equal_index);
    RUN_TEST(test_sync_reject_older_index);
    RUN_TEST(test_sync_accept_first_update_from_zero);
    RUN_TEST(test_sync_reject_zero_when_already_ahead);
    RUN_TEST(test_rollover_guard_passes_normal_value);
    RUN_TEST(test_rollover_guard_passes_at_threshold);
    RUN_TEST(test_rollover_guard_resets_above_threshold);
    RUN_TEST(test_rollover_guard_resets_max_uint32);
    RUN_TEST(test_sync_out_of_order_messages_do_not_regress);
    RUN_TEST(test_sync_duplicate_messages_ignored);
}
