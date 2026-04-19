#pragma once

// Pure button state machine — no hardware, no Arduino, natively testable.
//
// ButtonReading holds one poll tick's worth of hardware button state. It is
// produced by ObservableButton (from the hardware Button class) and consumed
// by ButtonStateMachine::update(). Keeping it as a plain struct lets native
// tests inject controlled readings without any hardware dependency.
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

// One poll tick's button readings. The hardware-side producer (ObservableButton)
// must call Button::read() before sampling these values. Tests construct this
// directly with known values.
struct ButtonReading {
    bool wasJustPressed    = false;
    bool heldPastLong      = false;
    bool heldPastExtraLong = false;
    bool wasJustReleased   = false;

    ButtonReading() = default;
    ButtonReading(bool pressed, bool longHeld, bool extraLongHeld, bool released)
        : wasJustPressed(pressed), heldPastLong(longHeld),
          heldPastExtraLong(extraLongHeld), wasJustReleased(released) {}
};

struct ButtonStateMachine {
    enum class State { Up, Pressed, LongPressed, ExtraLongPressed };
    enum class Event { None, Press, LongPress, ExtraLongPress };

    State state = State::Up;

    // Call once per poll tick with the readings for this tick.
    Event update(const ButtonReading& r) {
        if (r.wasJustPressed) {
            state = State::Pressed;
            return Event::None;
        }
        if (r.heldPastLong && state == State::Pressed) {
            state = State::LongPressed;
            return Event::LongPress;
        }
        if (r.heldPastExtraLong && state == State::LongPressed) {
            state = State::ExtraLongPressed;
            return Event::ExtraLongPress;
        }
        if (r.wasJustReleased) {
            State prev = state;
            state = State::Up;
            if (prev == State::Pressed) return Event::Press;
            return Event::None;
        }
        return Event::None;
    }
};

} // namespace Scarfnet
