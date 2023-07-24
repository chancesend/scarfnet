#include "ObservableButton.h"

namespace Scarf
{

#define kTaskCheckButtonPressIntervalMs (50) // in milliseconds

ObservableButton::ObservableButton(Scheduler* scheduler, uint8_t buttonPin) :
    _button(buttonPin, 0, 10),
    _taskCheckButtonPress( kTaskCheckButtonPressIntervalMs, TASK_FOREVER, 
        [&](){ this->checkButtonPress(); })
{
    scheduler->addTask( _taskCheckButtonPress );
    _taskCheckButtonPress.enable();
}

void ObservableButton::checkButtonPress()
{
    const int32_t kLongPressTimeMs = 1000;
    _button.read();
    if (_button.pressedFor(kLongPressTimeMs) && _button.isPressed() && buttonState != kButtonState_LongPressed)
    {
        buttonState = kButtonState_LongPressed;
        Serial.printf("Long press!\n");
        onEvent(Event::eLongPress);
    }
    else if( _button.wasPressed() ) {
        buttonState = kButtonState_Pressed;
        Serial.printf("Button press!\n");
        //selectNextPattern();
        onEvent(Event::ePress);
    }
    else if (_button.wasReleased())
    {
        buttonState = kButtonState_Up;
        onEvent(Event::eRelease);
    }
}

}
