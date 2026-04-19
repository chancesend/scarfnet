#pragma once

// Pure button state machine — no hardware, no Arduino, natively testable.
//
// Call update() once per poll tick with three boolean readings from the
// hardware button layer. Returns the event to fire (None if nothing changed).
//
// State transitions:
//   Up  ──wasJustPressed──►  Pressed  ──heldPastLong──►  LongPressed  ──heldPastExtraLong──►  ExtraLongPressed
//         (no event)                    (fires LongPress)                (fires ExtraLongPress)
//
//   Pressed  ──wasJustReleased──►  Up  (fires Press — short tap detected on release)
//   LongPressed ──wasJustReleased──►  Up  (no event — LongPress already fired while held)
//   ExtraLongPressed ──wasJustReleased──►  Up  (no event — in practice ESP.restart() runs first)

#include <cstdint>

namespace Scarfnet {

struct ButtonStateMachine {
    enum class State { Up, Pressed, LongPressed, ExtraLongPressed };
    enum class Event { None, Press, LongPress, ExtraLongPress };

    State state = State::Up;

    // Call once per poll tick.
    //   wasJustPressed   — button edge: up→down this tick (e.g. Button::wasPressed())
    //   heldPastLong     — button has been held ≥ kLongPressMs (e.g. Button::pressedFor(kLongPressMs))
    //   heldPastExtraLong — button has been held ≥ kExtraLongPressMs
    //   wasJustReleased  — button edge: down→up this tick (e.g. Button::wasReleased())
    Event update(bool wasJustPressed, bool heldPastLong,
                 bool heldPastExtraLong, bool wasJustReleased) {
        if (wasJustPressed) {
            state = State::Pressed;
            return Event::None;
        }
        if (heldPastLong && state == State::Pressed) {
            state = State::LongPressed;
            return Event::LongPress;
        }
        if (heldPastExtraLong && state == State::LongPressed) {
            state = State::ExtraLongPressed;
            return Event::ExtraLongPress;
        }
        if (wasJustReleased) {
            State prev = state;
            state = State::Up;
            if (prev == State::Pressed) return Event::Press;
            // Long/extra-long presses already fired while held — no event on release.
            return Event::None;
        }
        return Event::None;
    }
};

} // namespace Scarfnet
