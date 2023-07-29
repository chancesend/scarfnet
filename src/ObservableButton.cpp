#include "ObservableButton.h"

namespace Scarf
{

#define kTaskCheckButtonPressIntervalMs (50) // in milliseconds

ObservableButton::ObservableButton(Scheduler* scheduler, uint8_t buttonPin) :
    _scheduler(scheduler),
    _button(buttonPin, 1, 10),
    _taskCheckButtonEvent( kTaskCheckButtonPressIntervalMs, TASK_FOREVER, 
        [&](){ this->checkButtonEvent(); })
{
    Serial.printf("ObservableButton::ObservableButton()\n");
}

void ObservableButton::setup()
{
    _scheduler->addTask( _taskCheckButtonEvent );
    _taskCheckButtonEvent.enable();
}

void ObservableButton::checkButtonEvent()
{
    const int32_t kLongPressTimeMs = 1000;
    const auto instantState = _button.read();

    if( _button.wasPressed()) {
        buttonState = kButtonState_Pressed;
        Serial.printf("Button press!\n");
    }
    else if (_button.pressedFor(kLongPressTimeMs) && buttonState != kButtonState_LongPressed)
    {
        buttonState = kButtonState_LongPressed;
        Serial.printf("Long press!\n");
        onEvent(Event::eLongPress);
    }
    else if (_button.wasReleased() && buttonState == kButtonState_LongPressed)
    {
        buttonState = kButtonState_Up;
        Serial.printf("Button long press release!\n");
    }
    else if (_button.wasReleased() && buttonState == kButtonState_Pressed)
    {
        buttonState = kButtonState_Up;
        Serial.printf("Button short press release!\n");
        //selectNextPattern();
        onEvent(Event::ePress);
    }
}

}
