#pragma once

// Debounces raw button samples into the per-tick ButtonReading that
// ButtonStateMachine consumes. Hardware-free so it's natively testable; the
// embedded build feeds it digitalRead() and millis().
//
// A change in the raw level is taken at once, then further changes are
// ignored for the debounce window, so contact bounce can't register as extra
// presses. The first sample sets the state without reporting an edge, so a
// button already held at startup isn't seen as a fresh press.

#include "button_state_machine.h"
#include <stdint.h>

namespace Scarfnet {

class ButtonDebouncer
{
public:
    ButtonDebouncer(uint32_t debounceMs, uint32_t longPressMs, uint32_t extraLongPressMs)
        : _debounceMs(debounceMs), _longPressMs(longPressMs), _extraLongPressMs(extraLongPressMs) {}

    ButtonReading sample(bool rawPressed, uint32_t nowMs)
    {
        bool changed = false;
        if (!_started) {
            _started    = true;
            _pressed    = rawPressed;
            _lastChange = nowMs;
        } else if (nowMs - _lastChange >= _debounceMs && rawPressed != _pressed) {
            _pressed    = rawPressed;
            _lastChange = nowMs;
            changed     = true;
        }
        uint32_t heldMs = _pressed ? nowMs - _lastChange : 0;
        return ButtonReading{
            _pressed && changed,
            _pressed && heldMs >= _longPressMs,
            _pressed && heldMs >= _extraLongPressMs,
            !_pressed && changed,
        };
    }

private:
    uint32_t _debounceMs;
    uint32_t _longPressMs;
    uint32_t _extraLongPressMs;
    bool     _started    = false;
    bool     _pressed    = false;
    uint32_t _lastChange = 0;
};

} // namespace Scarfnet
