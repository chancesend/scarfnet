#include <unity.h>
#include <transition.h>

using Scarfnet::Transition;

void test_transition_idle_until_begun()
{
    Transition t(1000);
    uint8_t amt = 77;
    TEST_ASSERT_FALSE(t.step(5000, amt));
}

void test_transition_starts_on_first_step_after_begin()
{
    Transition t(1000);
    t.begin();
    uint8_t amt = 77;
    TEST_ASSERT_TRUE(t.step(5000, amt));
    TEST_ASSERT_EQUAL_UINT8(0, amt);
}

void test_transition_progresses_then_finishes()
{
    Transition t(1000);
    t.begin();
    uint8_t amt;
    t.step(5000, amt);
    TEST_ASSERT_TRUE(t.step(5500, amt));
    TEST_ASSERT_EQUAL_UINT8(127, amt);
    TEST_ASSERT_FALSE(t.step(6000, amt));
    TEST_ASSERT_FALSE(t.step(6001, amt));  // stays finished
}

void test_transition_armed_runs_without_begin()
{
    Transition t(1000, /*armed=*/true);
    uint8_t amt;
    TEST_ASSERT_TRUE(t.step(0, amt));
}

void test_transition_works_when_started_at_time_zero()
{
    // The old `startMs == 0` sentinel meant "inactive", so a fade starting at
    // t=0 never ran. The explicit state machine has no such sentinel.
    Transition t(1000);
    t.begin();
    uint8_t amt;
    TEST_ASSERT_TRUE(t.step(0, amt));
    TEST_ASSERT_TRUE(t.step(500, amt));
    TEST_ASSERT_EQUAL_UINT8(127, amt);
}

void test_transition_backward_clock_step_completes()
{
    // Clock stepping backward wraps elapsed to a huge value: fade snaps to done.
    Transition t(1000);
    t.begin();
    uint8_t amt;
    t.step(5000, amt);
    TEST_ASSERT_FALSE(t.step(4990, amt));
}

void test_transition_rebegin_restarts()
{
    Transition t(1000);
    t.begin();
    uint8_t amt;
    t.step(5000, amt);
    t.step(5900, amt);
    t.begin();  // new pattern change mid-fade
    TEST_ASSERT_TRUE(t.step(5950, amt));
    TEST_ASSERT_EQUAL_UINT8(0, amt);
}

void transition_tests()
{
    RUN_TEST(test_transition_idle_until_begun);
    RUN_TEST(test_transition_starts_on_first_step_after_begin);
    RUN_TEST(test_transition_progresses_then_finishes);
    RUN_TEST(test_transition_armed_runs_without_begin);
    RUN_TEST(test_transition_works_when_started_at_time_zero);
    RUN_TEST(test_transition_backward_clock_step_completes);
    RUN_TEST(test_transition_rebegin_restarts);
}
