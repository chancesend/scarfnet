#pragma once

#include "defines.h"
#include "button_state_machine.h"

#include <TaskScheduler.h>

#if SCARFNET_EMBEDDED
#include <Arduino.h>
#include <M5Stack.h>
#include <FastLED.h>
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
// Hardware reads are done through a PollFn. The default (hardware) constructor
// creates a PollFn that calls Button::read() before sampling state — so the
// read() call is structurally baked in and cannot be accidentally omitted.
// The test constructor accepts an injected PollFn, enabling native unit tests
// for the full ObservableButton → ButtonStateMachine → event dispatch path.
class ObservableButton
{
public:
    enum Event {
        ePress = 0,
        eLongPress,
        eDoublePress,
        eRelease,
        eExtraLongPress,
    };

    using EventCallback_t = std::function<void(const Event&)>;

    // A function that reads the hardware (or a mock) and returns the current
    // button state as a ButtonReading. Must call Button::read() before sampling.
    using PollFn = std::function<ButtonReading()>;

#if SCARFNET_EMBEDDED
    // Normal embedded constructor. Creates a PollFn that calls _button.read()
    // before sampling — the read() call cannot be omitted from this path.
    ObservableButton(Scheduler* scheduler, uint8_t buttonPin);
#endif

#if !SCARFNET_EMBEDDED
    // Test / injection constructor. Accepts an external PollFn so native tests
    // can drive the full event-dispatch chain without any hardware dependency.
    // Not available in embedded builds: Button _button has no default constructor,
    // and all real usage goes through the hardware constructor above.
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
    Button _button;
#endif

    using MyObserverList = std::vector<EventCallback_t>;
    MyObserverList _observers;
    Scheduler*     _scheduler;
};

}
