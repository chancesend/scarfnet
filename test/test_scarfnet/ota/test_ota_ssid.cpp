#include <unity.h>
#include <ota_ssid.h>

using Scarfnet::otaServerVersionFromSsid;

void test_ota_ssid_parses_version()
{
    TEST_ASSERT_EQUAL_INT(112, otaServerVersionFromSsid("scarfnet-ota-v112"));
    TEST_ASSERT_EQUAL_INT(7,   otaServerVersionFromSsid("scarfnet-ota-v7"));
}

void test_ota_ssid_rejects_other_networks()
{
    TEST_ASSERT_EQUAL_INT(-1, otaServerVersionFromSsid("HomeWiFi"));
    TEST_ASSERT_EQUAL_INT(-1, otaServerVersionFromSsid(""));
    TEST_ASSERT_EQUAL_INT(-1, otaServerVersionFromSsid("scarfnet-ota"));
}

void test_ota_ssid_rejects_malformed_versions()
{
    TEST_ASSERT_EQUAL_INT(-1, otaServerVersionFromSsid("scarfnet-ota-v"));
    TEST_ASSERT_EQUAL_INT(-1, otaServerVersionFromSsid("scarfnet-ota-v12x"));
    TEST_ASSERT_EQUAL_INT(-1, otaServerVersionFromSsid("scarfnet-ota-v-5"));
}

void ota_ssid_tests()
{
    RUN_TEST(test_ota_ssid_parses_version);
    RUN_TEST(test_ota_ssid_rejects_other_networks);
    RUN_TEST(test_ota_ssid_rejects_malformed_versions);
}
