#pragma once

#include "defines.h"

#if SCARFNET_EMBEDDED
#include <Arduino.h>
#include <M5Stack.h>

//#define FASTLED_INTERNAL (1)
#include <FastLED.h>
#endif

#include <list>
#include <stdint.h>
#include <functional>

namespace Scarfnet
{

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

    enum ButtonState
    {
        kButtonState_Up = 0,
        kButtonState_Pressed,
        kButtonState_LongPressed,
        kButtonState_DoublePressed,
        kButtonState_ExtraLongPressed,
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
    
    void setup();

private:
    void checkButtonEvent();

    Task _taskCheckButtonEvent;
    ButtonState _buttonState = kButtonState_Up;
    Button _button;

    typedef std::vector<EventCallback_t> MyObserverList;
    MyObserverList _observers;
    Scheduler* _scheduler;
};

};