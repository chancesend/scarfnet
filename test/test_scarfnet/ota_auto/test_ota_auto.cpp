#include <unity.h>
#include <ota_auto.h>

using Scarfnet::autoOtaHoldoffMs;
using Scarfnet::autoOtaMayScan;

void test_ota_auto_first_attempt_is_immediate()
{
    TEST_ASSERT_EQUAL_UINT32(0, autoOtaHoldoffMs(0));
    TEST_ASSERT_TRUE(autoOtaMayScan(true, 0, 0));
}

void test_ota_auto_needs_a_newer_peer()
{
    TEST_ASSERT_FALSE(autoOtaMayScan(false, 0, 1000000));
}

void test_ota_auto_holdoff_grows_per_failure()
{
    TEST_ASSERT_EQUAL_UINT32(30000, autoOtaHoldoffMs(1));
    TEST_ASSERT_EQUAL_UINT32(45000, autoOtaHoldoffMs(2));
    TEST_ASSERT_EQUAL_UINT32(60000, autoOtaHoldoffMs(3));
    TEST_ASSERT_EQUAL_UINT32(225000, autoOtaHoldoffMs(14));
    TEST_ASSERT_EQUAL_UINT32(240000, autoOtaHoldoffMs(15));
}

void test_ota_auto_holdoff_is_capped()
{
    TEST_ASSERT_EQUAL_UINT32(kOtaAutoBackoffMaxMs, autoOtaHoldoffMs(30));
    TEST_ASSERT_EQUAL_UINT32(kOtaAutoBackoffMaxMs, autoOtaHoldoffMs(UINT32_MAX));
}

void test_ota_auto_waits_out_holdoff_since_boot()
{
    uint32_t holdoff = autoOtaHoldoffMs(2);
    TEST_ASSERT_FALSE(autoOtaMayScan(true, 2, holdoff - 1));
    TEST_ASSERT_TRUE(autoOtaMayScan(true, 2, holdoff));
}

void ota_auto_tests()
{
    RUN_TEST(test_ota_auto_first_attempt_is_immediate);
    RUN_TEST(test_ota_auto_needs_a_newer_peer);
    RUN_TEST(test_ota_auto_holdoff_grows_per_failure);
    RUN_TEST(test_ota_auto_holdoff_is_capped);
    RUN_TEST(test_ota_auto_waits_out_holdoff_since_boot);
}
