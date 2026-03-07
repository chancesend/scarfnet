#include <unity.h>

void test_mesh_dummy2() {
    TEST_ASSERT_TRUE(3 == 2);
}

void mesh_tests() {
    RUN_TEST(test_mesh_dummy2);
}