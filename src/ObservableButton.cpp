#include "ObservableButton.h"
#include "config.h"
#include "log.h"

namespace Scarfnet
{

ObservableButton::ObservableButton(Scheduler *scheduler, uint8_t buttonPin) : _scheduler(scheduler),
                                                                                _button(buttonPin, 1, 10),
                                                                                _taskCheckButtonEvent(kButtonPollIntervalMs, TASK_FOREVER,
                                                                                                    [&]()
                                                                                                    { this->checkButtonEvent(); })
{
    Scarfnet::log("ObservableButton::ObservableButton()");
}

void ObservableButton::setup()
{
    _scheduler->addTask(_taskCheckButtonEvent);
    _taskCheckButtonEvent.enable();
}

void ObservableButton::checkButtonEvent()
{
    const auto instantState = _button.read();

    if (_button.wasPressed())
    {
        _buttonState = kButtonState_Pressed;
        Scarfnet::log("Button press!");
    }
    else if (_button.pressedFor(kLongPressMs) && _buttonState != kButtonState_LongPressed)
    {
        _buttonState = kButtonState_LongPressed;
        Scarfnet::log("Long press!");
        onEvent(Event::eLongPress);
    }
    else if (_button.pressedFor(kExtraLongPressMs) && _buttonState != kButtonState_ExtraLongPressed)
    {
        _buttonState = kButtonState_ExtraLongPressed;
        Scarfnet::log("Extra-long press!");
        onEvent(Event::eExtraLongPress);
    }
    else if (_button.wasReleased() && _buttonState == kButtonState_LongPressed)
    {
        _buttonState = kButtonState_Up;
        Scarfnet::log("Button long press release!");
    }
    else if (_button.wasReleased() && _buttonState == kButtonState_Pressed)
    {
        _buttonState = kButtonState_Up;
        Scarfnet::log("Button short press release!");
        onEvent(Event::ePress);
    }
}

}
