#include <unity.h>
#include <sync.h>
#include <sync_state.h>

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

// ---------------------------------------------------------------------------
// Relay round-trip: a scarf that misses the press and hears it second-hand must
// end up with the presser's full state, not the relayer's.
// ---------------------------------------------------------------------------

// Minimal model of Scarf's send/receive path for the replicated state.
struct RelayNode
{
    Scarfnet::SyncState state;

    Scarfnet::HeartbeatPacket heartbeat() const
    {
        Scarfnet::HeartbeatPacket pkt = {};
        Scarfnet::writeSyncState(pkt, state);
        return pkt;
    }

    void receive(const Scarfnet::HeartbeatPacket& pkt)
    {
        if (Scarfnet::shouldAcceptUpdate(pkt.changeIndex, state.changeIndex))
            Scarfnet::adoptSyncState(state, pkt);
    }
};

void test_sync_state_round_trips_through_packet()
{
    Scarfnet::SyncState in = {6, 5000, 42, 0xBEEF};
    Scarfnet::HeartbeatPacket pkt = {};
    Scarfnet::writeSyncState(pkt, in);

    Scarfnet::SyncState out = {};
    Scarfnet::adoptSyncState(out, pkt);
    TEST_ASSERT_EQUAL_UINT32(6u, out.changeIndex);
    TEST_ASSERT_EQUAL_UINT32(5000u, out.lastPressMs);
    TEST_ASSERT_EQUAL_UINT32(42u, out.lastPressId);
    TEST_ASSERT_EQUAL_HEX16(0xBEEF, out.randomizer);
}

void test_sync_relayed_update_carries_pressers_randomizer()
{
    // All three synced at ci=5. B has never pressed, so its randomizer is 0.
    RelayNode presser{{5, 1000, 1, 0x1111}};
    RelayNode relay  {{5, 1000, 1, 0x0000}};
    RelayNode missed {{5, 1000, 1, 0x1111}};

    // Presser presses: new randomizer, ci=6.
    presser.state = {6, 5000, 1, 0xBEEF};

    // Relay hears the press directly; missed is out of range.
    relay.receive(presser.heartbeat());

    // Missed comes back and hears the relay's heartbeat first.
    missed.receive(relay.heartbeat());
    TEST_ASSERT_EQUAL_UINT32(6u, missed.state.changeIndex);
    TEST_ASSERT_EQUAL_HEX16(0xBEEF, missed.state.randomizer);

    // Equal ci: the presser's own later heartbeats can't correct a mismatch,
    // which is why it would otherwise persist until the next press.
    TEST_ASSERT_FALSE(Scarfnet::shouldAcceptUpdate(presser.heartbeat().changeIndex,
                                                   missed.state.changeIndex));
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
    RUN_TEST(test_sync_state_round_trips_through_packet);
    RUN_TEST(test_sync_relayed_update_carries_pressers_randomizer);
}
