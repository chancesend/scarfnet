#include <unity.h>
#include <heartbeat_framer.h>

#include <cstring>

using Scarfnet::HeartbeatFramer;
using Scarfnet::HeartbeatPacket;

// ─── decode ─────────────────────────────────────────────────────────────────

void test_framer_decode_short_packet_returns_false()
{
    uint8_t buf[1] = {0};
    HeartbeatPacket out = {};
    TEST_ASSERT_FALSE(HeartbeatFramer::decode(buf, 1, out));
}

void test_framer_decode_empty_packet_returns_false()
{
    HeartbeatPacket out = {};
    TEST_ASSERT_FALSE(HeartbeatFramer::decode(nullptr, 0, out));
}

void test_framer_decode_exact_size_succeeds()
{
    HeartbeatPacket src = {};
    src.id            = 0x12345678;
    src.lastPress     = 9999;
    src.currentTimeMs = 100000;
    src.changeIndex   = 7;
    src.randomizer    = 42;
    memcpy(src.pattern, "pride", 6);  // 5 chars + null

    HeartbeatPacket out = {};
    bool ok = HeartbeatFramer::decode(
        reinterpret_cast<const uint8_t*>(&src), (int)sizeof(src), out);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, out.id);
    TEST_ASSERT_EQUAL_UINT32(9999,       out.lastPress);
    TEST_ASSERT_EQUAL_UINT32(100000,     out.currentTimeMs);
    TEST_ASSERT_EQUAL_UINT32(7,          out.changeIndex);
    TEST_ASSERT_EQUAL_UINT8(42,          out.randomizer);
    TEST_ASSERT_EQUAL_STRING("pride",    out.pattern);
}

void test_framer_decode_larger_buffer_succeeds()
{
    // Decode should still work when len > sizeof(HeartbeatPacket).
    HeartbeatPacket src = {};
    src.id = 0xDEADBEEF;
    memcpy(src.pattern, "colorwaves", 11);

    uint8_t bigBuf[300] = {};
    memcpy(bigBuf, &src, sizeof(src));

    HeartbeatPacket out = {};
    TEST_ASSERT_TRUE(HeartbeatFramer::decode(bigBuf, 300, out));
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, out.id);
    TEST_ASSERT_EQUAL_STRING("colorwaves", out.pattern);
}

void test_framer_decode_null_terminates_unterminated_pattern()
{
    // Simulate a packet whose pattern field has no null terminator.
    HeartbeatPacket src = {};
    memset(src.pattern, 'X', sizeof(src.pattern));  // all 'X', no null

    HeartbeatPacket out = {};
    HeartbeatFramer::decode(reinterpret_cast<const uint8_t*>(&src), (int)sizeof(src), out);

    // decode() must force the last byte to '\0'.
    TEST_ASSERT_EQUAL_UINT8('\0', (uint8_t)out.pattern[sizeof(out.pattern) - 1]);
}

// ─── encode ─────────────────────────────────────────────────────────────────

void test_framer_encode_returns_correct_length()
{
    HeartbeatPacket pkt = {};
    int len = 0;
    HeartbeatFramer::encode(pkt, len);
    TEST_ASSERT_EQUAL_INT((int)sizeof(HeartbeatPacket), len);
}

void test_framer_encode_returns_packet_bytes()
{
    HeartbeatPacket pkt = {};
    pkt.id = 0xCAFEBABE;
    pkt.changeIndex = 3;
    memcpy(pkt.pattern, "cylon", 6);

    int len = 0;
    const uint8_t* wire = HeartbeatFramer::encode(pkt, len);

    // Round-trip: decode the encoded bytes and verify fields.
    HeartbeatPacket decoded = {};
    TEST_ASSERT_TRUE(HeartbeatFramer::decode(wire, len, decoded));
    TEST_ASSERT_EQUAL_UINT32(0xCAFEBABE, decoded.id);
    TEST_ASSERT_EQUAL_UINT32(3,          decoded.changeIndex);
    TEST_ASSERT_EQUAL_STRING("cylon",    decoded.pattern);
}

// ─── round-trip ─────────────────────────────────────────────────────────────

void test_framer_round_trip_preserves_all_fields()
{
    HeartbeatPacket orig = {};
    orig.id            = 0x11223344;
    orig.lastPress     = 55555;
    orig.currentTimeMs = 1000000;
    orig.changeIndex   = 99;
    orig.randomizer    = 200;
    memcpy(orig.pattern, "firework", 9);

    int len = 0;
    const uint8_t* wire = HeartbeatFramer::encode(orig, len);

    HeartbeatPacket decoded = {};
    TEST_ASSERT_TRUE(HeartbeatFramer::decode(wire, len, decoded));

    TEST_ASSERT_EQUAL_UINT32(orig.id,            decoded.id);
    TEST_ASSERT_EQUAL_UINT32(orig.lastPress,     decoded.lastPress);
    TEST_ASSERT_EQUAL_UINT32(orig.currentTimeMs, decoded.currentTimeMs);
    TEST_ASSERT_EQUAL_UINT32(orig.changeIndex,   decoded.changeIndex);
    TEST_ASSERT_EQUAL_UINT8 (orig.randomizer,    decoded.randomizer);
    TEST_ASSERT_EQUAL_STRING(orig.pattern,       decoded.pattern);
}

// ---------------------------------------------------------------------------

void heartbeat_framer_tests()
{
    RUN_TEST(test_framer_decode_short_packet_returns_false);
    RUN_TEST(test_framer_decode_empty_packet_returns_false);
    RUN_TEST(test_framer_decode_exact_size_succeeds);
    RUN_TEST(test_framer_decode_larger_buffer_succeeds);
    RUN_TEST(test_framer_decode_null_terminates_unterminated_pattern);
    RUN_TEST(test_framer_encode_returns_correct_length);
    RUN_TEST(test_framer_encode_returns_packet_bytes);
    RUN_TEST(test_framer_round_trip_preserves_all_fields);
}
