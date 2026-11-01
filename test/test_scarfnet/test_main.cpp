#include <unity.h>

#include "beat_info/test_beat_info.h"
#include "builtin_led/test_builtin_led.h"
#include "clock_sync/test_clock_sync.h"
#include "heartbeat_framer/test_heartbeat_framer.h"
#include "node_tracker/test_node_tracker.h"
#include "observable_button/test_ObservableButton.h"
#include "ota/test_ota_ssid.h"
#include "patterns/test_patterns.h"
#include "patterns/test_cylon.h"
#include "sync/test_sync.h"
#include "tap_tempo/test_tap_tempo.h"
#include "tempo_sync/test_tempo_sync.h"
#include "transition/test_transition.h"

void setUp(void) {}

void tearDown(void) {}

int main( int argc, char **argv) {
    UNITY_BEGIN();

    beat_info_tests();
    builtin_led_tests();
    clock_sync_tests();
    heartbeat_framer_tests();
    node_tracker_tests();
    observable_button_tests();
    ota_ssid_tests();
    patterns_tests();
    cylon_tests();
    sync_tests();
    tap_tempo_tests();
    tempo_sync_tests();
    transition_tests();
    
    UNITY_END();
}