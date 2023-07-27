#include <unity.h>

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_simple() {
    TEST_ASSERT_TRUE(1 == 1);
}

int main( int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_simple);
    UNITY_END();
}