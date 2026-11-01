#pragma once

#include "defines.h"
#include "button_state_machine.h"
#include "button_debouncer.h"
#include "config.h"

#include <TaskScheduler.h>

#if SCARFNET_EMBEDDED
#include <Arduino.h>
#endif

#include <functional>
#include <vector>
#include <stdint.h>

namespace Scarfnet
{

// Polls a hardware button on a TaskScheduler task and classifies presses into
// discrete events. Observers are notified synchronously on the scheduler task.
//
// The state machine logic lives in ButtonStateMachine (button_state_machine.h)
// and is unit-tested independently. This class is the hardware glue layer.
//
// Hardware reads are done through a PollFn: the hardware constructor samples
// the pin through a ButtonDebouncer; the test constructor accepts an injected
// PollFn, so native tests drive the full event-dispatch path.
class ObservableButton
{
public:
    enum Event {
        eDown = 0,      // fires on the press edge (button physically goes down)
        ePress,         // fires on the release edge after a short press
        eLongPress,
        eExtraLongPress,
    };

    using EventCallback_t = std::function<void(const Event&)>;

    // Reads the hardware (or a mock) and returns this tick's ButtonReading.
    using PollFn = std::function<ButtonReading()>;

#if SCARFNET_EMBEDDED
    // Active-low button on `buttonPin`.
    ObservableButton(Scheduler* scheduler, uint8_t buttonPin);
#endif

#if !SCARFNET_EMBEDDED
    // Test / injection constructor. Accepts an external PollFn so native tests
    // can drive the full event-dispatch chain without any hardware dependency.
    ObservableButton(Scheduler* scheduler, PollFn pollFn);
#endif

    // Registers a callback invoked on each button event.
    void addObserver(const EventCallback_t& observer)
    {
        _observers.push_back(observer);
    }
    void onEvent(const Event& event)
    {
        for (const auto& observer : _observers)
            observer(event);
    }

    // Adds the polling task to the scheduler. Must be called once during setup.
    void setup();

private:
    void checkButtonEvent();

    PollFn             _pollFn;
    ButtonStateMachine _sm;
    Task               _taskCheckButtonEvent;

#if SCARFNET_EMBEDDED
    uint8_t         _buttonPin;
    ButtonDebouncer _debouncer {kButtonDebounceMs, kLongPressMs, kExtraLongPressMs};
#endif

    using MyObserverList = std::vector<EventCallback_t>;
    MyObserverList _observers;
    Scheduler*     _scheduler;
};

}
