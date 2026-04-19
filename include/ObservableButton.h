#pragma once

#include "defines.h"
#include "button_state_machine.h"

#include <TaskScheduler.h>

#if SCARFNET_EMBEDDED
#include <Arduino.h>
#include <M5Stack.h>
#include <FastLED.h>
#endif

#include <list>
#include <stdint.h>
#include <functional>
#include <vector>

namespace Scarfnet
{

// Polls a hardware button on a TaskScheduler task and classifies presses into
// discrete events. Observers are notified synchronously on the scheduler task.
//
// The state machine logic lives in ButtonStateMachine (button_state_machine.h)
// and is unit-tested independently. This class is the hardware glue layer.
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
    typedef std::function<void(const Event&)> EventCallback_t;

    ObservableButton(Scheduler* scheduler, uint8_t buttonPin);

    // Registers a callback invoked on each button event.
    void addObserver(const EventCallback_t& observer)
    {
        _observers.push_back(observer);
    }
    void onEvent(const Event& event)
    {
        for (const auto& observer : _observers)
        {
            observer(event);
        }
    }

    // Adds the polling task to the scheduler. Must be called once during setup.
    void setup();

private:
    void checkButtonEvent();

    Task _taskCheckButtonEvent;
    ButtonStateMachine _sm;
    Button _button;

    typedef std::vector<EventCallback_t> MyObserverList;
    MyObserverList _observers;
    Scheduler* _scheduler;
};

}
