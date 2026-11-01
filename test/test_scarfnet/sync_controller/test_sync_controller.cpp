#include <unity.h>
#include <sync_controller.h>
#include <heartbeat_builder.h>
#include <node_flash.h>

#include <string>
#include <vector>

using namespace Scarfnet;

// ---------------------------------------------------------------------------
// Helpers: a scarf is a SyncController plus the pattern it is showing.
// ---------------------------------------------------------------------------

static const char* const kKnownPatterns[] = {"pride", "confetti", "cylon", "sparkle"};
static const int kKnownCount = 4;

static bool isKnown(const char* name)
{
    for (const char* p : kKnownPatterns)
        if (std::string(p) == name) return true;
    return false;
}

struct TestScarf {
    NodeId         id;
    SyncController sync;
    std::string    pattern = "pride";
    int            applyCalls = 0;

    explicit TestScarf(NodeId i) : id(i) {}

    HeartbeatPacket heartbeat(TimeMs now) const
    {
        return buildHeartbeat(id, now, sync.state(), pattern, sync.tempo());
    }

    SyncController::Received hear(const HeartbeatPacket& pkt)
    {
        return sync.receive(pkt, pattern.c_str(), [this](const char* name, uint16_t) {
            ++applyCalls;
            if (!isKnown(name)) return false;
            pattern = name;
            return true;
        });
    }

    void press(TimeMs now, uint16_t randomizer)
    {
        sync.localPress(now, id, randomizer);
        pattern = kKnownPatterns[randomizer % kKnownCount];
    }
};

static bool sameLook(const TestScarf& a, const TestScarf& b)
{
    return a.sync.state().changeIndex == b.sync.state().changeIndex &&
           a.sync.state().randomizer  == b.sync.state().randomizer  &&
           a.sync.state().lastPressId == b.sync.state().lastPressId &&
           a.pattern == b.pattern;
}

// ---------------------------------------------------------------------------
// Single heartbeat decisions
// ---------------------------------------------------------------------------

void test_sync_controller_accepts_newer_and_rebroadcasts()
{
    TestScarf presser(1), listener(2);
    presser.press(1000, 7);
    auto r = listener.hear(presser.heartbeat(1000));
    TEST_ASSERT_TRUE(r.decision == UpdateDecision::Accept);
    TEST_ASSERT_TRUE(r.rebroadcast);
    TEST_ASSERT_FALSE(r.unknownPattern);
    TEST_ASSERT_TRUE(sameLook(presser, listener));
}

void test_sync_controller_ignores_duplicate_silently()
{
    TestScarf presser(1), listener(2);
    presser.press(1000, 7);
    listener.hear(presser.heartbeat(1000));
    int calls = listener.applyCalls;
    auto r = listener.hear(presser.heartbeat(6000));
    TEST_ASSERT_TRUE(r.decision == UpdateDecision::Ignore);
    TEST_ASSERT_FALSE(r.rebroadcast);
    TEST_ASSERT_EQUAL(calls, listener.applyCalls);
}

void test_sync_controller_unknown_pattern_not_adopted()
{
    TestScarf listener(2);
    TestScarf presser(1);
    presser.press(1000, 7);
    HeartbeatPacket pkt = presser.heartbeat(1000);
    setPatternName(pkt, "from-newer-firmware");
    auto r = listener.hear(pkt);
    TEST_ASSERT_TRUE(r.decision == UpdateDecision::Accept);
    TEST_ASSERT_TRUE(r.unknownPattern);
    TEST_ASSERT_FALSE(r.rebroadcast);
    TEST_ASSERT_EQUAL_UINT32(0, listener.sync.state().changeIndex);
    // Still newer next time, so it retries.
    TEST_ASSERT_TRUE(listener.hear(pkt).unknownPattern);
}

void test_sync_controller_flags_differing_older_state()
{
    TestScarf a(1), b(2);
    a.press(1000, 7);
    a.press(2000, 9);   // a at ci=2
    b.press(1500, 11);  // b at ci=1, different look
    auto r = a.hear(b.heartbeat(2000));
    TEST_ASSERT_TRUE(r.decision == UpdateDecision::RejectDiffering);
    TEST_ASSERT_FALSE(r.rebroadcast);
}

void test_sync_controller_notes_newer_firmware()
{
    TestScarf a(1), b(2);
    HeartbeatPacket pkt = b.heartbeat(0);
    TEST_ASSERT_FALSE(a.sync.newerVersionSeen());
    pkt.version = kScarfVersion + 1;
    a.hear(pkt);
    TEST_ASSERT_TRUE(a.sync.newerVersionSeen());
}

void test_sync_controller_newer_tempo_rebroadcasts()
{
    TestScarf tapper(1), listener(2);
    tapper.sync.toggleTapMode();
    tapper.sync.tap(1000);
    tapper.sync.tap(1500);
    auto r = listener.hear(tapper.heartbeat(1600));
    TEST_ASSERT_TRUE(r.tempo == TempoSync::Result::Adopted);
    TEST_ASSERT_TRUE(r.decision == UpdateDecision::Ignore);
    TEST_ASSERT_TRUE(r.rebroadcast);
}

void test_sync_controller_local_press_claims_next_index()
{
    TestScarf a(42);
    a.sync.localPress(1234, 42, 99);
    TEST_ASSERT_EQUAL_UINT32(1, a.sync.state().changeIndex);
    TEST_ASSERT_EQUAL_UINT32(1234, a.sync.state().lastPressMs);
    TEST_ASSERT_EQUAL_UINT32(42, a.sync.state().lastPressId);
    TEST_ASSERT_EQUAL_UINT16(99, a.sync.state().randomizer);
}

// ---------------------------------------------------------------------------
// Many scarves on a lossy broadcast network
// ---------------------------------------------------------------------------

struct Lcg {
    uint32_t s;
    uint32_t next() { s = s * 1664525u + 1013904223u; return s >> 8; }
    bool chance(uint32_t pct) { return next() % 100 < pct; }
};

struct Network {
    std::vector<TestScarf> scarves;
    Lcg      rng;
    uint32_t lossPct;
    TimeMs   now = 0;
    std::vector<size_t> pending;  // senders queued this round (presses, rebroadcasts)

    Network(int n, uint32_t loss, uint32_t seed) : rng{seed}, lossPct(loss)
    {
        for (int i = 0; i < n; ++i) scarves.emplace_back(0x1000 + (NodeId)i);
    }

    // One sender's broadcast; each receiver independently may miss it.
    void broadcast(size_t from, const std::vector<bool>& deaf)
    {
        HeartbeatPacket pkt = scarves[from].heartbeat(now);
        for (size_t to = 0; to < scarves.size(); ++to) {
            if (to == from || deaf[to] || rng.chance(lossPct)) continue;
            if (scarves[to].hear(pkt).rebroadcast) pending.push_back(to);
        }
    }

    // Every scarf heartbeats once; rebroadcasts and presses flood within the round.
    void round(std::vector<bool> deaf = {})
    {
        deaf.resize(scarves.size(), false);
        now += kHeartbeatIntervalMs;
        for (size_t i = 0; i < scarves.size(); ++i) pending.push_back(i);
        for (size_t k = 0; k < pending.size() && k < 10000; ++k) broadcast(pending[k], deaf);
        pending.clear();
    }

    void press(size_t who)
    {
        scarves[who].press(now, (uint16_t)rng.next());
        pending.push_back(who);
    }

    bool converged() const
    {
        for (const auto& s : scarves)
            if (!sameLook(s, scarves[0])) return false;
        return true;
    }
};

void test_sync_network_converges_under_heavy_loss()
{
    Network net(8, 50, 12345);
    for (int r = 0; r < 60; ++r) {
        if (net.rng.chance(40)) net.press(net.rng.next() % 8);
        if (net.rng.chance(15)) net.press(net.rng.next() % 8);  // sometimes simultaneous
        net.round();
    }
    for (int r = 0; r < 20; ++r) net.round();
    TEST_ASSERT_TRUE(net.converged());
    TEST_ASSERT_TRUE(net.scarves[0].sync.state().changeIndex > 0);
}

void test_sync_network_simultaneous_presses_resolve_identically()
{
    Network net(6, 30, 777);
    net.round();
    for (size_t i = 0; i < 6; ++i) net.press(i);  // everyone presses from the same index
    for (int r = 0; r < 10; ++r) net.round();
    TEST_ASSERT_TRUE(net.converged());
}

void test_sync_network_scarf_that_missed_pushes_catches_up()
{
    // The Burning Man symptom: a scarf misses a run of updates entirely.
    Network net(5, 20, 4242);
    std::vector<bool> deaf(5, false);
    deaf[3] = true;
    for (int r = 0; r < 15; ++r) {
        net.press(r % 3);
        net.round(deaf);
    }
    TEST_ASSERT_FALSE(sameLook(net.scarves[3], net.scarves[0]));
    for (int r = 0; r < 5; ++r) net.round();
    TEST_ASSERT_TRUE(net.converged());
}

void test_sync_network_stale_scarf_cannot_roll_back_the_fleet()
{
    Network net(4, 0, 9);
    net.press(0);
    net.round();
    // A fresh scarf (rebooted, changeIndex 0) joins and must adopt, not override.
    net.scarves.emplace_back(0x2000);
    for (int r = 0; r < 3; ++r) net.round();
    TEST_ASSERT_TRUE(net.converged());
    TEST_ASSERT_EQUAL_UINT32(1, net.scarves[4].sync.state().changeIndex);
}

// ---------------------------------------------------------------------------
// NodeFlashTable
// ---------------------------------------------------------------------------

void test_node_flash_refreshes_existing_sender()
{
    NodeFlashTable t;
    t.record(1, 100);
    t.record(1, 200);
    TEST_ASSERT_EQUAL(1, t.count);
    TEST_ASSERT_EQUAL_UINT32(200, t.entries[0].when);
}

void test_node_flash_full_table_replaces_oldest_across_wrap()
{
    NodeFlashTable t;
    for (int i = 0; i < kMaxNodeFlashes; ++i)
        t.record((NodeId)i, 0xFFFFFF00u + (TimeMs)i);  // oldest is id 0
    t.record(0, 5);            // id 0 refreshed past the wrap: now the newest
    t.record(999, 6);          // replaces id 1, the oldest
    TEST_ASSERT_EQUAL(kMaxNodeFlashes, t.count);
    TEST_ASSERT_EQUAL_UINT32(999, t.entries[1].id);
    TEST_ASSERT_EQUAL_UINT32(0, t.entries[0].id);
}

void sync_controller_tests()
{
    RUN_TEST(test_sync_controller_accepts_newer_and_rebroadcasts);
    RUN_TEST(test_sync_controller_ignores_duplicate_silently);
    RUN_TEST(test_sync_controller_unknown_pattern_not_adopted);
    RUN_TEST(test_sync_controller_flags_differing_older_state);
    RUN_TEST(test_sync_controller_notes_newer_firmware);
    RUN_TEST(test_sync_controller_newer_tempo_rebroadcasts);
    RUN_TEST(test_sync_controller_local_press_claims_next_index);
    RUN_TEST(test_sync_network_converges_under_heavy_loss);
    RUN_TEST(test_sync_network_simultaneous_presses_resolve_identically);
    RUN_TEST(test_sync_network_scarf_that_missed_pushes_catches_up);
    RUN_TEST(test_sync_network_stale_scarf_cannot_roll_back_the_fleet);
    RUN_TEST(test_node_flash_refreshes_existing_sender);
    RUN_TEST(test_node_flash_full_table_replaces_oldest_across_wrap);
}
