#include <unity.h>
#include <node_tracker.h>

using Scarfnet::NodeTracker;
using Scarfnet::NodeId;

// ---------------------------------------------------------------------------
// saw() — join detection
// ---------------------------------------------------------------------------

void test_node_tracker_new_node_returns_true()
{
    NodeTracker t;
    TEST_ASSERT_TRUE(t.saw(0xAABBCCDD, 1000));
}

void test_node_tracker_known_node_returns_false()
{
    NodeTracker t;
    t.saw(0xAABBCCDD, 1000);
    TEST_ASSERT_FALSE(t.saw(0xAABBCCDD, 2000));
}

void test_node_tracker_two_different_nodes_both_new()
{
    NodeTracker t;
    TEST_ASSERT_TRUE(t.saw(0x00000001, 1000));
    TEST_ASSERT_TRUE(t.saw(0x00000002, 1000));
    TEST_ASSERT_EQUAL_INT(2, (int)t.count());
}

void test_node_tracker_count_increments_on_join()
{
    NodeTracker t;
    TEST_ASSERT_EQUAL_INT(0, (int)t.count());
    t.saw(0x00000001, 100);
    TEST_ASSERT_EQUAL_INT(1, (int)t.count());
    t.saw(0x00000002, 200);
    TEST_ASSERT_EQUAL_INT(2, (int)t.count());
}

void test_node_tracker_count_stable_on_repeat_heartbeat()
{
    NodeTracker t;
    t.saw(0x00000001, 100);
    t.saw(0x00000001, 200);
    t.saw(0x00000001, 300);
    TEST_ASSERT_EQUAL_INT(1, (int)t.count());
}

// ---------------------------------------------------------------------------
// checkTimeouts() — leave synthesis
// ---------------------------------------------------------------------------

void test_node_tracker_timeout_fires_leave_callback()
{
    NodeTracker t;
    t.saw(0xDEADBEEF, 0);

    NodeId evicted = 0;
    int removed = t.checkTimeouts(16000, 15000, [&](NodeId id) { evicted = id; });

    TEST_ASSERT_EQUAL_INT(1, removed);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, evicted);
}

void test_node_tracker_timeout_removes_peer_from_count()
{
    NodeTracker t;
    t.saw(0x00000001, 0);
    TEST_ASSERT_EQUAL_INT(1, (int)t.count());

    t.checkTimeouts(16000, 15000, nullptr);
    TEST_ASSERT_EQUAL_INT(0, (int)t.count());
}

void test_node_tracker_alive_node_not_timed_out()
{
    NodeTracker t;
    t.saw(0x00000001, 0);

    // Only 5s have passed — under the 15s timeout.
    int removed = t.checkTimeouts(5000, 15000, nullptr);
    TEST_ASSERT_EQUAL_INT(0, removed);
    TEST_ASSERT_EQUAL_INT(1, (int)t.count());
}

void test_node_tracker_only_stale_peers_evicted()
{
    NodeTracker t;
    t.saw(0x00000001, 0);      // stale — last seen at 0
    t.saw(0x00000002, 15000);  // fresh — last seen at 15 000ms

    // now=20 000, timeout=15 000 → node 1 is 20s old (evicted), node 2 is 5s old (kept)
    int removed = t.checkTimeouts(20000, 15000, nullptr);
    TEST_ASSERT_EQUAL_INT(1, removed);
    TEST_ASSERT_EQUAL_INT(1, (int)t.count());
}

void test_node_tracker_heartbeat_refreshes_timeout()
{
    NodeTracker t;
    t.saw(0x00000001, 0);
    // Refresh at 10s
    t.saw(0x00000001, 10000);
    // Check at 20s — node was refreshed at 10s, so it's only 10s old (kept)
    int removed = t.checkTimeouts(20000, 15000, nullptr);
    TEST_ASSERT_EQUAL_INT(0, removed);
}

void test_node_tracker_timeout_exactly_at_boundary()
{
    NodeTracker t;
    t.saw(0x00000001, 0);
    // nowMs - lastSeenMs == timeoutMs → NOT timed out (uses strict >)
    int removed = t.checkTimeouts(15000, 15000, nullptr);
    TEST_ASSERT_EQUAL_INT(0, removed);
}

void test_node_tracker_no_callback_called_when_no_timeout()
{
    NodeTracker t;
    t.saw(0x00000001, 0);
    bool called = false;
    t.checkTimeouts(5000, 15000, [&](NodeId) { called = true; });
    TEST_ASSERT_FALSE(called);
}

// ---------------------------------------------------------------------------

void node_tracker_tests()
{
    RUN_TEST(test_node_tracker_new_node_returns_true);
    RUN_TEST(test_node_tracker_known_node_returns_false);
    RUN_TEST(test_node_tracker_two_different_nodes_both_new);
    RUN_TEST(test_node_tracker_count_increments_on_join);
    RUN_TEST(test_node_tracker_count_stable_on_repeat_heartbeat);
    RUN_TEST(test_node_tracker_timeout_fires_leave_callback);
    RUN_TEST(test_node_tracker_timeout_removes_peer_from_count);
    RUN_TEST(test_node_tracker_alive_node_not_timed_out);
    RUN_TEST(test_node_tracker_only_stale_peers_evicted);
    RUN_TEST(test_node_tracker_heartbeat_refreshes_timeout);
    RUN_TEST(test_node_tracker_timeout_exactly_at_boundary);
    RUN_TEST(test_node_tracker_no_callback_called_when_no_timeout);
}
