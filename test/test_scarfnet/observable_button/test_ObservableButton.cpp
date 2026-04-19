#include <unity.h>
#include <button_state_machine.h>

using Scarfnet::ButtonStateMachine;
using State = ButtonStateMachine::State;
using Event = ButtonStateMachine::Event;

// Shorthand: simulate one poll tick. 0/false = not happening this tick.
static Event tick(ButtonStateMachine& sm,
                  bool pressed = false, bool longHeld = false,
                  bool extraLongHeld = false, bool released = false) {
    return sm.update(pressed, longHeld, extraLongHeld, released);
}

// ─── Initial state ───────────────────────────────────────────────────────────

void test_button_sm_initial_state_is_up()
{
    ButtonStateMachine sm;
    TEST_ASSERT_EQUAL_INT((int)State::Up, (int)sm.state);
}

void test_button_sm_idle_tick_fires_no_event()
{
    ButtonStateMachine sm;
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)tick(sm));
}

// ─── Short press (tap) ───────────────────────────────────────────────────────

void test_button_sm_press_then_release_fires_press_event()
{
    ButtonStateMachine sm;
    // Tick 1: wasJustPressed
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)tick(sm, /*pressed=*/true));
    TEST_ASSERT_EQUAL_INT((int)State::Pressed, (int)sm.state);
    // Tick 2: released before long threshold
    TEST_ASSERT_EQUAL_INT((int)Event::Press, (int)tick(sm, false, false, false, /*released=*/true));
    TEST_ASSERT_EQUAL_INT((int)State::Up, (int)sm.state);
}

void test_button_sm_press_event_fires_on_release_not_on_press()
{
    ButtonStateMachine sm;
    // No event on the down edge
    Event downEvent = tick(sm, true);
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)downEvent);
    // Event fires on the up edge
    Event upEvent = tick(sm, false, false, false, true);
    TEST_ASSERT_EQUAL_INT((int)Event::Press, (int)upEvent);
}

void test_button_sm_returns_to_up_after_short_press()
{
    ButtonStateMachine sm;
    tick(sm, true);
    tick(sm, false, false, false, true);
    TEST_ASSERT_EQUAL_INT((int)State::Up, (int)sm.state);
    // Further ticks do nothing
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)tick(sm));
}

// ─── Long press ──────────────────────────────────────────────────────────────

void test_button_sm_long_press_fires_while_held()
{
    ButtonStateMachine sm;
    tick(sm, true);                          // down
    tick(sm);                                // held, not yet long
    Event e = tick(sm, false, /*longHeld=*/true);  // threshold crossed
    TEST_ASSERT_EQUAL_INT((int)Event::LongPress, (int)e);
    TEST_ASSERT_EQUAL_INT((int)State::LongPressed, (int)sm.state);
}

void test_button_sm_long_press_fires_only_once()
{
    ButtonStateMachine sm;
    tick(sm, true);
    tick(sm, false, true);  // fires LongPress, state → LongPressed
    // Subsequent ticks with longHeld still true must not re-fire
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)tick(sm, false, true));
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)tick(sm, false, true));
}

void test_button_sm_long_press_release_fires_no_event()
{
    ButtonStateMachine sm;
    tick(sm, true);
    tick(sm, false, true);  // LongPress fired
    Event e = tick(sm, false, false, false, true);  // release
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)e);
    TEST_ASSERT_EQUAL_INT((int)State::Up, (int)sm.state);
}

void test_button_sm_long_press_requires_pressed_state()
{
    // Long threshold must not fire if no wasJustPressed was seen first.
    ButtonStateMachine sm;
    // Feed longHeld without a prior press edge — should be ignored.
    Event e = tick(sm, false, true);
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)e);
    TEST_ASSERT_EQUAL_INT((int)State::Up, (int)sm.state);
}

// ─── Extra-long press ────────────────────────────────────────────────────────

void test_button_sm_extra_long_press_fires_after_long()
{
    ButtonStateMachine sm;
    tick(sm, true);                     // down → Pressed
    tick(sm, false, true);              // long threshold → LongPressed
    Event e = tick(sm, false, true, true);  // extra-long threshold
    TEST_ASSERT_EQUAL_INT((int)Event::ExtraLongPress, (int)e);
    TEST_ASSERT_EQUAL_INT((int)State::ExtraLongPressed, (int)sm.state);
}

void test_button_sm_extra_long_press_fires_only_once()
{
    ButtonStateMachine sm;
    tick(sm, true);
    tick(sm, false, true);      // LongPressed
    tick(sm, false, true, true);  // ExtraLongPressed, fires
    // Further ticks must not re-fire
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)tick(sm, false, true, true));
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)tick(sm, false, true, true));
}

void test_button_sm_extra_long_requires_long_press_state()
{
    // Extra-long threshold seen while still in Pressed state → must not fire.
    // In practice this can't happen (ExtraLong > Long), but the state machine
    // must be robust against out-of-order inputs.
    ButtonStateMachine sm;
    tick(sm, true);  // Pressed
    // Skip long press; jump straight to extraLong = true
    Event e = tick(sm, false, false, true);
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)e);
    TEST_ASSERT_EQUAL_INT((int)State::Pressed, (int)sm.state);
}

void test_button_sm_long_does_not_refire_after_extra_long()
{
    // After ExtraLongPressed, longHeld=true must not re-trigger LongPress.
    ButtonStateMachine sm;
    tick(sm, true);
    tick(sm, false, true);       // LongPressed
    tick(sm, false, true, true); // ExtraLongPressed
    Event e = tick(sm, false, true, true);  // both thresholds still held
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)e);
    TEST_ASSERT_EQUAL_INT((int)State::ExtraLongPressed, (int)sm.state);
}

// ─── Full sequences ──────────────────────────────────────────────────────────

void test_button_sm_full_short_press_sequence()
{
    ButtonStateMachine sm;
    TEST_ASSERT_EQUAL_INT((int)Event::None,  (int)tick(sm, true));           // down
    TEST_ASSERT_EQUAL_INT((int)Event::None,  (int)tick(sm));                  // held
    TEST_ASSERT_EQUAL_INT((int)Event::None,  (int)tick(sm));                  // held
    TEST_ASSERT_EQUAL_INT((int)Event::Press, (int)tick(sm, false,false,false,true)); // release
    TEST_ASSERT_EQUAL_INT((int)State::Up,    (int)sm.state);
}

void test_button_sm_full_long_press_sequence()
{
    ButtonStateMachine sm;
    TEST_ASSERT_EQUAL_INT((int)Event::None,      (int)tick(sm, true));
    TEST_ASSERT_EQUAL_INT((int)Event::LongPress, (int)tick(sm, false, true));
    TEST_ASSERT_EQUAL_INT((int)Event::None,      (int)tick(sm, false, true)); // still held
    TEST_ASSERT_EQUAL_INT((int)Event::None,      (int)tick(sm, false,false,false,true)); // release
    TEST_ASSERT_EQUAL_INT((int)State::Up,        (int)sm.state);
}

void test_button_sm_full_extra_long_press_sequence()
{
    ButtonStateMachine sm;
    TEST_ASSERT_EQUAL_INT((int)Event::None,           (int)tick(sm, true));
    TEST_ASSERT_EQUAL_INT((int)Event::LongPress,      (int)tick(sm, false, true));
    TEST_ASSERT_EQUAL_INT((int)Event::ExtraLongPress, (int)tick(sm, false, true, true));
    TEST_ASSERT_EQUAL_INT((int)Event::None,           (int)tick(sm, false,false,false,true)); // release
    TEST_ASSERT_EQUAL_INT((int)State::Up,             (int)sm.state);
}

void test_button_sm_two_sequential_short_presses()
{
    ButtonStateMachine sm;
    // First press
    tick(sm, true);
    TEST_ASSERT_EQUAL_INT((int)Event::Press, (int)tick(sm, false,false,false,true));
    // Second press
    tick(sm, true);
    TEST_ASSERT_EQUAL_INT((int)Event::Press, (int)tick(sm, false,false,false,true));
}

// ─── Edge cases ──────────────────────────────────────────────────────────────

void test_button_sm_release_with_no_prior_press_is_ignored()
{
    ButtonStateMachine sm;
    // wasJustReleased without prior press edge — state stays Up, no event.
    Event e = tick(sm, false, false, false, true);
    TEST_ASSERT_EQUAL_INT((int)Event::None, (int)e);
    TEST_ASSERT_EQUAL_INT((int)State::Up,   (int)sm.state);
}

void test_button_sm_multiple_idle_ticks_do_not_change_state()
{
    ButtonStateMachine sm;
    for (int i = 0; i < 20; ++i) {
        TEST_ASSERT_EQUAL_INT((int)Event::None, (int)tick(sm));
        TEST_ASSERT_EQUAL_INT((int)State::Up,   (int)sm.state);
    }
}

// ---------------------------------------------------------------------------

void observable_button_tests()
{
    RUN_TEST(test_button_sm_initial_state_is_up);
    RUN_TEST(test_button_sm_idle_tick_fires_no_event);
    RUN_TEST(test_button_sm_press_then_release_fires_press_event);
    RUN_TEST(test_button_sm_press_event_fires_on_release_not_on_press);
    RUN_TEST(test_button_sm_returns_to_up_after_short_press);
    RUN_TEST(test_button_sm_long_press_fires_while_held);
    RUN_TEST(test_button_sm_long_press_fires_only_once);
    RUN_TEST(test_button_sm_long_press_release_fires_no_event);
    RUN_TEST(test_button_sm_long_press_requires_pressed_state);
    RUN_TEST(test_button_sm_extra_long_press_fires_after_long);
    RUN_TEST(test_button_sm_extra_long_press_fires_only_once);
    RUN_TEST(test_button_sm_extra_long_requires_long_press_state);
    RUN_TEST(test_button_sm_long_does_not_refire_after_extra_long);
    RUN_TEST(test_button_sm_full_short_press_sequence);
    RUN_TEST(test_button_sm_full_long_press_sequence);
    RUN_TEST(test_button_sm_full_extra_long_press_sequence);
    RUN_TEST(test_button_sm_two_sequential_short_presses);
    RUN_TEST(test_button_sm_release_with_no_prior_press_is_ignored);
    RUN_TEST(test_button_sm_multiple_idle_ticks_do_not_change_state);
}
