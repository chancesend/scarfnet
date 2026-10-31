#include <unity.h>
#include <sync.h>
#include <sync_state.h>
#include <heartbeat_builder.h>

#include <string.h>
#include <string>

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
        if (Scarfnet::isNewerState(Scarfnet::syncStateOf(pkt), state))
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

    // Missed now holds exactly the presser's state, so the presser's own later
    // heartbeats are plain duplicates.
    TEST_ASSERT_FALSE(Scarfnet::isNewerState(Scarfnet::syncStateOf(presser.heartbeat()),
                                             missed.state));
}

// ---------------------------------------------------------------------------
// isNewerState: equal-changeIndex tie-break (simultaneous presses)
// ---------------------------------------------------------------------------

static Scarfnet::HeartbeatPacket packetWith(Scarfnet::ChangeIndex ci, const char* pattern);

void test_newer_state_higher_index_wins_regardless_of_tiebreak_fields()
{
    Scarfnet::SyncState local    = {5, 0, 999, 0xFFFF};
    Scarfnet::SyncState incoming = {6, 0,   1, 0x0001};
    TEST_ASSERT_TRUE(Scarfnet::isNewerState(incoming, local));
    TEST_ASSERT_FALSE(Scarfnet::isNewerState(local, incoming));
}

void test_newer_state_equal_index_higher_randomizer_wins()
{
    // Randomizer decides before presser id, so the lower-id scarf can win.
    Scarfnet::SyncState low  = {6, 0, 900, 0x1000};
    Scarfnet::SyncState high = {6, 0, 100, 0x2000};
    TEST_ASSERT_TRUE(Scarfnet::isNewerState(high, low));
    TEST_ASSERT_FALSE(Scarfnet::isNewerState(low, high));
}

void test_newer_state_equal_index_and_randomizer_higher_presser_wins()
{
    Scarfnet::SyncState a = {6, 0, 100, 0x1234};
    Scarfnet::SyncState b = {6, 0, 200, 0x1234};
    TEST_ASSERT_TRUE(Scarfnet::isNewerState(b, a));
    TEST_ASSERT_FALSE(Scarfnet::isNewerState(a, b));
}

void test_newer_state_identical_is_not_newer()
{
    Scarfnet::SyncState s = {6, 5000, 100, 0x1234};
    TEST_ASSERT_FALSE(Scarfnet::isNewerState(s, s));
}

void test_newer_state_fresh_boot_states_are_duplicates()
{
    // Two freshly booted scarves both carry the all-zero default state.
    Scarfnet::SyncState boot = {0, 0, 0, 0};
    TEST_ASSERT_FALSE(Scarfnet::isNewerState(boot, boot));
}

// The field split: A and B press from ci=5 at the same moment. Each other scarf
// keeps whichever press reaches it first. After one round of heartbeats every
// scarf must hold the same state — the tie-break winner.
void test_sync_simultaneous_presses_converge_on_one_winner()
{
    const Scarfnet::SyncState pressA = {6, 5000, /*id=*/100, /*rnd=*/0x9000};
    const Scarfnet::SyncState pressB = {6, 5003, /*id=*/900, /*rnd=*/0x4000};

    RelayNode a{pressA};
    RelayNode b{pressB};
    RelayNode c{{5, 1000, 7, 0x1111}};
    RelayNode d{{5, 1000, 7, 0x1111}};

    // C hears A first, D hears B first: the network is split.
    c.receive(a.heartbeat());
    d.receive(b.heartbeat());
    c.receive(b.heartbeat());
    d.receive(a.heartbeat());

    // One round of everyone hearing everyone.
    RelayNode* nodes[] = {&a, &b, &c, &d};
    Scarfnet::HeartbeatPacket beats[4];
    for (int i = 0; i < 4; ++i) beats[i] = nodes[i]->heartbeat();
    for (auto* n : nodes)
        for (auto& pkt : beats) n->receive(pkt);

    // A wins on randomizer even though B has the higher id.
    for (auto* n : nodes) {
        TEST_ASSERT_EQUAL_UINT32(6u, n->state.changeIndex);
        TEST_ASSERT_EQUAL_HEX16(0x9000, n->state.randomizer);
        TEST_ASSERT_EQUAL_UINT32(100u, n->state.lastPressId);
    }
}

void test_classify_equal_index_winning_tiebreak_accepted()
{
    Scarfnet::SyncState local = {6, 0, 100, 0x1000};
    Scarfnet::HeartbeatPacket pkt = packetWith(6, "fractal");
    pkt.globalRandomizer = 0x2000;
    pkt.lastPressId      = 200;
    TEST_ASSERT_TRUE(Scarfnet::classifyUpdate(pkt, local, "cylon")
                     == Scarfnet::UpdateDecision::Accept);
}

void test_classify_same_pattern_different_randomizer_is_differing()
{
    // Same pattern name but a different variation still looks different.
    Scarfnet::SyncState local = {6, 0, 100, 0x2000};
    Scarfnet::HeartbeatPacket pkt = packetWith(6, "cylon");
    pkt.globalRandomizer = 0x1000;  // loses the tie-break
    pkt.lastPressId      = 200;
    TEST_ASSERT_TRUE(Scarfnet::classifyUpdate(pkt, local, "cylon")
                     == Scarfnet::UpdateDecision::RejectDiffering);
}

// ---------------------------------------------------------------------------
// classifyUpdate: what onReceived does with a heartbeat
// ---------------------------------------------------------------------------

static Scarfnet::HeartbeatPacket packetWith(Scarfnet::ChangeIndex ci, const char* pattern)
{
    Scarfnet::HeartbeatPacket pkt = {};
    pkt.changeIndex = ci;
    Scarfnet::setPatternName(pkt, pattern);
    return pkt;
}

void test_classify_newer_index_accepted()
{
    Scarfnet::SyncState local = {5, 0, 0, 0};
    TEST_ASSERT_TRUE(Scarfnet::classifyUpdate(packetWith(6, "fractal"), local, "cylon")
                     == Scarfnet::UpdateDecision::Accept);
}

void test_classify_same_index_same_pattern_ignored()
{
    Scarfnet::SyncState local = {5, 0, 0, 0};
    TEST_ASSERT_TRUE(Scarfnet::classifyUpdate(packetWith(5, "cylon"), local, "cylon")
                     == Scarfnet::UpdateDecision::Ignore);
}

void test_classify_same_index_different_pattern_rejected_differing()
{
    // Equal-index tie: two scarves disagree and neither can win — stuck.
    Scarfnet::SyncState local = {5, 0, 0, 0};
    TEST_ASSERT_TRUE(Scarfnet::classifyUpdate(packetWith(5, "fractal"), local, "cylon")
                     == Scarfnet::UpdateDecision::RejectDiffering);
}

void test_classify_older_index_different_pattern_rejected_differing()
{
    // A lagging sender: we're ahead, it's the stuck one.
    Scarfnet::SyncState local = {5, 0, 0, 0};
    TEST_ASSERT_TRUE(Scarfnet::classifyUpdate(packetWith(4, "fractal"), local, "cylon")
                     == Scarfnet::UpdateDecision::RejectDiffering);
}

// ---------------------------------------------------------------------------
// buildHeartbeat / setPatternName
// ---------------------------------------------------------------------------

void test_build_heartbeat_fills_all_fields()
{
    Scarfnet::TempoSync tempo;
    tempo.tempo.setFromPacket(500, 10000, 100);  // last beat at 9900
    tempo.index = 17;
    Scarfnet::SyncState sync = {6, 5000, 42, 0xBEEF};

    Scarfnet::HeartbeatPacket pkt =
        Scarfnet::buildHeartbeat(7, 10200, sync, "fractal", tempo);

    TEST_ASSERT_EQUAL_UINT32(7u, pkt.id);
    TEST_ASSERT_EQUAL_UINT16(kScarfVersion, pkt.version);
    TEST_ASSERT_EQUAL_UINT32(10200u, pkt.currentTimeMs);
    TEST_ASSERT_EQUAL_UINT32(6u, pkt.changeIndex);
    TEST_ASSERT_EQUAL_UINT32(5000u, pkt.lastPressMs);
    TEST_ASSERT_EQUAL_UINT32(42u, pkt.lastPressId);
    TEST_ASSERT_EQUAL_HEX16(0xBEEF, pkt.globalRandomizer);
    TEST_ASSERT_EQUAL_STRING("fractal", pkt.pattern);
    TEST_ASSERT_EQUAL_UINT16(17, pkt.tempoIndex);
    TEST_ASSERT_EQUAL_UINT16(500, pkt.beatIntervalMs);
    TEST_ASSERT_EQUAL_UINT16(300, pkt.beatPhaseMs);  // (10200 - 9900) % 500
}

void test_set_pattern_name_truncates_and_terminates()
{
    Scarfnet::HeartbeatPacket pkt = {};
    memset(pkt.pattern, 'x', sizeof(pkt.pattern));
    Scarfnet::setPatternName(pkt, std::string(40, 'a'));
    TEST_ASSERT_EQUAL_size_t(31, strlen(pkt.pattern));
    TEST_ASSERT_EQUAL_CHAR('\0', pkt.pattern[31]);
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
    RUN_TEST(test_classify_newer_index_accepted);
    RUN_TEST(test_classify_same_index_same_pattern_ignored);
    RUN_TEST(test_classify_same_index_different_pattern_rejected_differing);
    RUN_TEST(test_classify_older_index_different_pattern_rejected_differing);
    RUN_TEST(test_build_heartbeat_fills_all_fields);
    RUN_TEST(test_set_pattern_name_truncates_and_terminates);
    RUN_TEST(test_newer_state_higher_index_wins_regardless_of_tiebreak_fields);
    RUN_TEST(test_newer_state_equal_index_higher_randomizer_wins);
    RUN_TEST(test_newer_state_equal_index_and_randomizer_higher_presser_wins);
    RUN_TEST(test_newer_state_identical_is_not_newer);
    RUN_TEST(test_newer_state_fresh_boot_states_are_duplicates);
    RUN_TEST(test_sync_simultaneous_presses_converge_on_one_winner);
    RUN_TEST(test_classify_equal_index_winning_tiebreak_accepted);
    RUN_TEST(test_classify_same_pattern_different_randomizer_is_differing);
}
