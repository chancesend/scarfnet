#pragma once

#include "defines.h"

#include <Arduino.h>
#include <M5Stack.h>
#include <FastLED.h>

#include <list>
#include <stdint.h>
#include <functional>

namespace Scarf
{

class ObservableButton
{
public:
        enum Event {
            ePress = 0,
            eLongPress,
            eDoublePress,
            eRelease,
        };
    typedef std::function<void(const Event&)> EventCallback_t;

    enum ButtonState
    {
        kButtonState_Up = 0,
        kButtonState_Pressed,
        kButtonState_LongPressed,
        kButtonState_DoublePressed,
    };

    ObservableButton(Scheduler* scheduler, uint8_t buttonPin);

    void addObserver(const EventCallback_t& observer)
    {
        _observers.push_back(observer);
    }
    void onEvent(const Event& event)
    {
        for(const auto& observer: _observers)
        {
            observer(event);
        }
    }

private:
    void checkButtonPress();

    Task _taskCheckButtonPress;
    ButtonState buttonState = kButtonState_Up;
    Button _button;

    typedef std::vector<EventCallback_t> MyObserverList;
    MyObserverList _observers;
};

};