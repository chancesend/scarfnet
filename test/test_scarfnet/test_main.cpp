#include <unity.h>

#include "mesh/test_mesh.h"
#include "observable_button/test_ObservableButton.h"
#include "patterns/test_patterns.h"
#include "scheduler/test_scheduler.h"
#include "sync/test_sync.h"

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
    mesh_tests();
    observable_button_tests();
    patterns_tests();
    scheduler_tests();
    sync_tests();
    
    UNITY_END();
}