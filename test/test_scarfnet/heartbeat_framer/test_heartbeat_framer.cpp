#include <unity.h>
#include <heartbeat_framer.h>

#include <cstring>

using Scarfnet::HeartbeatFramer;
using Scarfnet::HeartbeatPacket;

// ─── crc32 ───────────────────────────────────────────────────────────────────

void test_framer_crc32_known_value()
{
    // Standard CRC-32 test vector: "123456789" → 0xCBF43926
    const uint8_t input[] = {0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39};
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926u, HeartbeatFramer::crc32(input, sizeof(input)));
}

void test_framer_crc32_empty_is_zero()
{
    // Zero-length input: loop never runs, init XOR final = 0.
    TEST_ASSERT_EQUAL_UINT32(0u, HeartbeatFramer::crc32(nullptr, 0));
}

// ─── decode ──────────────────────────────────────────────────────────────────

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
    memcpy(src.pattern, "pride", 6);

    int wireLen = 0;
    const uint8_t* wire = HeartbeatFramer::encode(src, wireLen);

    HeartbeatPacket out = {};
    TEST_ASSERT_TRUE(HeartbeatFramer::decode(wire, wireLen, out));
    TEST_ASSERT_EQUAL_UINT32(0x12345678, out.id);
    TEST_ASSERT_EQUAL_UINT32(9999,       out.lastPress);
    TEST_ASSERT_EQUAL_UINT32(100000,     out.currentTimeMs);
    TEST_ASSERT_EQUAL_UINT32(7,          out.changeIndex);
    TEST_ASSERT_EQUAL_UINT16(42,         out.randomizer);
    TEST_ASSERT_EQUAL_STRING("pride",    out.pattern);
}

void test_framer_decode_larger_buffer_succeeds()
{
    // decode should still work when len > sizeof(HeartbeatPacket).
    HeartbeatPacket src = {};
    src.id = 0xDEADBEEF;
    memcpy(src.pattern, "colorwaves", 11);

    int wireLen = 0;
    HeartbeatFramer::encode(src, wireLen);

    uint8_t bigBuf[300] = {};
    memcpy(bigBuf, &src, sizeof(src));

    HeartbeatPacket out = {};
    TEST_ASSERT_TRUE(HeartbeatFramer::decode(bigBuf, 300, out));
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF,  out.id);
    TEST_ASSERT_EQUAL_STRING("colorwaves", out.pattern);
}

void test_framer_decode_null_terminates_unterminated_pattern()
{
    // Simulate a packet whose pattern field has no null terminator but a valid CRC.
    HeartbeatPacket src = {};
    memset(src.pattern, 'X', sizeof(src.pattern));  // all 'X', no null
    src.crc32 = 0;
    src.crc32 = HeartbeatFramer::crc32(
        reinterpret_cast<const uint8_t*>(&src), sizeof(src));

    HeartbeatPacket out = {};
    TEST_ASSERT_TRUE(HeartbeatFramer::decode(
        reinterpret_cast<const uint8_t*>(&src), (int)sizeof(src), out));
    // decode() must force the last byte to '\0'.
    TEST_ASSERT_EQUAL_UINT8('\0', (uint8_t)out.pattern[sizeof(out.pattern) - 1]);
}

void test_framer_decode_rejects_corrupt_data()
{
    HeartbeatPacket src = {};
    src.id = 0xABCD1234;
    memcpy(src.pattern, "dance", 6);

    int wireLen = 0;
    HeartbeatFramer::encode(src, wireLen);

    // Flip a data byte after stamping the CRC.
    uint8_t buf[sizeof(HeartbeatPacket)];
    memcpy(buf, &src, sizeof(src));
    buf[0] ^= 0xFF;  // corrupt the id field

    HeartbeatPacket out = {};
    TEST_ASSERT_FALSE(HeartbeatFramer::decode(buf, (int)sizeof(buf), out));
}

void test_framer_decode_rejects_wrong_crc()
{
    HeartbeatPacket src = {};
    src.id = 0x11111111;
    memcpy(src.pattern, "breathe", 8);

    int wireLen = 0;
    HeartbeatFramer::encode(src, wireLen);  // stamp correct CRC

    // Tamper with the CRC field directly.
    src.crc32 ^= 0xDEAD;

    HeartbeatPacket out = {};
    TEST_ASSERT_FALSE(HeartbeatFramer::decode(
        reinterpret_cast<const uint8_t*>(&src), (int)sizeof(src), out));
}

void test_framer_decode_zeroes_out_on_failure()
{
    // On CRC mismatch, `out` should be left zeroed.
    HeartbeatPacket src = {};
    src.id = 0xBADBADBAD;
    // Don't stamp CRC — crc32 stays 0 but computed value won't be 0.

    HeartbeatPacket out = {};
    out.id = 0xFFFFFFFF;  // pre-fill with non-zero
    HeartbeatFramer::decode(
        reinterpret_cast<const uint8_t*>(&src), (int)sizeof(src), out);
    TEST_ASSERT_EQUAL_UINT32(0, out.id);
}

// ─── encode ──────────────────────────────────────────────────────────────────

void test_framer_encode_returns_correct_length()
{
    HeartbeatPacket pkt = {};
    int len = 0;
    HeartbeatFramer::encode(pkt, len);
    TEST_ASSERT_EQUAL_INT((int)sizeof(HeartbeatPacket), len);
}

void test_framer_encode_stamps_nonzero_crc()
{
    // A non-trivial packet should produce a nonzero CRC.
    HeartbeatPacket pkt = {};
    pkt.id = 0xCAFEBABE;
    pkt.changeIndex = 3;
    memcpy(pkt.pattern, "cylon", 6);

    int len = 0;
    HeartbeatFramer::encode(pkt, len);
    TEST_ASSERT_NOT_EQUAL(0, pkt.crc32);
}

void test_framer_encode_crc_changes_with_content()
{
    HeartbeatPacket a = {}, b = {};
    a.id = 1;
    b.id = 2;
    int len = 0;
    HeartbeatFramer::encode(a, len);
    HeartbeatFramer::encode(b, len);
    TEST_ASSERT_NOT_EQUAL(a.crc32, b.crc32);
}

void test_framer_encode_returns_packet_bytes()
{
    HeartbeatPacket pkt = {};
    pkt.id = 0xCAFEBABE;
    pkt.changeIndex = 3;
    memcpy(pkt.pattern, "cylon", 6);

    int len = 0;
    const uint8_t* wire = HeartbeatFramer::encode(pkt, len);

    HeartbeatPacket decoded = {};
    TEST_ASSERT_TRUE(HeartbeatFramer::decode(wire, len, decoded));
    TEST_ASSERT_EQUAL_UINT32(0xCAFEBABE, decoded.id);
    TEST_ASSERT_EQUAL_UINT32(3,          decoded.changeIndex);
    TEST_ASSERT_EQUAL_STRING("cylon",    decoded.pattern);
}

// ─── round-trip ──────────────────────────────────────────────────────────────

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
    TEST_ASSERT_EQUAL_UINT16(orig.randomizer,    decoded.randomizer);
    TEST_ASSERT_EQUAL_STRING(orig.pattern,       decoded.pattern);
}

// ---------------------------------------------------------------------------

void heartbeat_framer_tests()
{
    // crc32
    RUN_TEST(test_framer_crc32_known_value);
    RUN_TEST(test_framer_crc32_empty_is_zero);

    // decode
    RUN_TEST(test_framer_decode_short_packet_returns_false);
    RUN_TEST(test_framer_decode_empty_packet_returns_false);
    RUN_TEST(test_framer_decode_exact_size_succeeds);
    RUN_TEST(test_framer_decode_larger_buffer_succeeds);
    RUN_TEST(test_framer_decode_null_terminates_unterminated_pattern);
    RUN_TEST(test_framer_decode_rejects_corrupt_data);
    RUN_TEST(test_framer_decode_rejects_wrong_crc);
    RUN_TEST(test_framer_decode_zeroes_out_on_failure);

    // encode
    RUN_TEST(test_framer_encode_returns_correct_length);
    RUN_TEST(test_framer_encode_stamps_nonzero_crc);
    RUN_TEST(test_framer_encode_crc_changes_with_content);
    RUN_TEST(test_framer_encode_returns_packet_bytes);

    // round-trip
    RUN_TEST(test_framer_round_trip_preserves_all_fields);
}
