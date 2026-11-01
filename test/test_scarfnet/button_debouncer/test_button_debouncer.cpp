#include <unity.h>
#include <button_debouncer.h>

using Scarfnet::ButtonDebouncer;
using Scarfnet::ButtonReading;

static ButtonDebouncer make() { return ButtonDebouncer(10, 1000, 7000); }

void test_debouncer_reports_press_and_release_edges()
{
    auto d = make();
    d.sample(false, 0);
    ButtonReading r = d.sample(true, 50);
    TEST_ASSERT_TRUE(r.wasJustPressed);
    TEST_ASSERT_FALSE(d.sample(true, 100).wasJustPressed);  // edge only once
    r = d.sample(false, 150);
    TEST_ASSERT_TRUE(r.wasJustReleased);
    TEST_ASSERT_FALSE(d.sample(false, 200).wasJustReleased);
}

void test_debouncer_ignores_bounce_inside_window()
{
    auto d = make();
    d.sample(false, 0);
    TEST_ASSERT_TRUE(d.sample(true, 50).wasJustPressed);
    ButtonReading r = d.sample(false, 55);  // bounce 5 ms after the edge
    TEST_ASSERT_FALSE(r.wasJustReleased);
    TEST_ASSERT_FALSE(d.sample(true, 58).wasJustPressed);
}

void test_debouncer_reports_long_and_extra_long_holds()
{
    auto d = make();
    d.sample(false, 0);
    d.sample(true, 100);
    TEST_ASSERT_FALSE(d.sample(true, 1099).heldPastLong);
    ButtonReading r = d.sample(true, 1100);
    TEST_ASSERT_TRUE(r.heldPastLong);
    TEST_ASSERT_FALSE(r.heldPastExtraLong);
    TEST_ASSERT_TRUE(d.sample(true, 7100).heldPastExtraLong);
    TEST_ASSERT_FALSE(d.sample(false, 7150).heldPastLong);
}

void test_debouncer_held_at_startup_is_not_a_press()
{
    auto d = make();
    TEST_ASSERT_FALSE(d.sample(true, 0).wasJustPressed);
    TEST_ASSERT_TRUE(d.sample(false, 50).wasJustReleased);
}

void test_debouncer_hold_survives_millis_wrap()
{
    auto d = make();
    d.sample(false, 0xFFFFFF00u);
    d.sample(true, 0xFFFFFF80u);
    TEST_ASSERT_TRUE(d.sample(true, 0xFFFFFF80u + 1000u).heldPastLong);  // wraps past 0
}

void button_debouncer_tests()
{
    RUN_TEST(test_debouncer_reports_press_and_release_edges);
    RUN_TEST(test_debouncer_ignores_bounce_inside_window);
    RUN_TEST(test_debouncer_reports_long_and_extra_long_holds);
    RUN_TEST(test_debouncer_held_at_startup_is_not_a_press);
    RUN_TEST(test_debouncer_hold_survives_millis_wrap);
}
