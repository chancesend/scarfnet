#include "ObservableButton.h"
#include "log.h"

namespace Scarfnet
{

const unsigned long kTaskCheckButtonPressIntervalMs = 50; // in milliseconds

ObservableButton::ObservableButton(Scheduler *scheduler, uint8_t buttonPin) : _scheduler(scheduler),
                                                                                _button(buttonPin, 1, 10),
                                                                                _taskCheckButtonEvent(kTaskCheckButtonPressIntervalMs, TASK_FOREVER,
                                                                                                    [&]()
                                                                                                    { this->checkButtonEvent(); })
{
    Serial.printf("ObservableButton::ObservableButton()\n");
}

void ObservableButton::setup()
{
    _scheduler->addTask(_taskCheckButtonEvent);
    _taskCheckButtonEvent.enable();
}

void ObservableButton::checkButtonEvent()
{
    const int32_t kLongPressTimeMs = 1000;
    const int32_t kExtraLongPressTimeMs = 7000;
    const auto instantState = _button.read();

    if (_button.wasPressed())
    {
        _buttonState = kButtonState_Pressed;
        Serial.printf("Button press!\n");
    }
    else if (_button.pressedFor(kLongPressTimeMs) && _buttonState != kButtonState_LongPressed)
    {
        _buttonState = kButtonState_LongPressed;
        Serial.printf("Long press!\n");
        onEvent(Event::eLongPress);
    }
    else if (_button.pressedFor(kExtraLongPressTimeMs) && _buttonState != kButtonState_ExtraLongPressed)
    {
        _buttonState = kButtonState_ExtraLongPressed;
        Serial.printf("Extra-long press!\n");
        onEvent(Event::eExtraLongPress);
    }
    else if (_button.wasReleased() && _buttonState == kButtonState_LongPressed)
    {
        _buttonState = kButtonState_Up;
        Serial.printf("Button long press release!\n");
    }
    else if (_button.wasReleased() && _buttonState == kButtonState_Pressed)
    {
        _buttonState = kButtonState_Up;
        Serial.printf("Button short press release!\n");
        // selectNextPattern();
        onEvent(Event::ePress);
    }
}

}
