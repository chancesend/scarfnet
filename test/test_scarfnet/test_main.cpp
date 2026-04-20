#include <unity.h>

#include "clock_sync/test_clock_sync.h"
#include "heartbeat_framer/test_heartbeat_framer.h"
#include "mesh/test_mesh.h"
#include "node_tracker/test_node_tracker.h"
#include "observable_button/test_ObservableButton.h"
#include "patterns/test_patterns.h"
#include "swarm/test_swarm_ema.h"
#include "sync/test_sync.h"
#include "tap_tempo/test_tap_tempo.h"

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
    clock_sync_tests();
    heartbeat_framer_tests();
    node_tracker_tests();
    mesh_tests();
    observable_button_tests();
    patterns_tests();
    swarm_ema_tests();
    sync_tests();
    tap_tempo_tests();
    
    UNITY_END();
}