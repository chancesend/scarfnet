#include "ObservableButton.h"
#include "config.h"
#include "log.h"

namespace Scarfnet
{

ObservableButton::ObservableButton(Scheduler *scheduler, uint8_t buttonPin)
    : _scheduler(scheduler),
      _button(buttonPin, 1, 10),
      _taskCheckButtonEvent(kButtonPollIntervalMs, TASK_FOREVER,
                            [&]() { this->checkButtonEvent(); })
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
    auto smEvent = _sm.update(
        _button.wasPressed(),
        _button.pressedFor(kLongPressMs),
        _button.pressedFor(kExtraLongPressMs),
        _button.wasReleased()
    );

    switch (smEvent) {
        case ButtonStateMachine::Event::Press:
            Scarfnet::log("Button short press!");
            onEvent(Event::ePress);
            break;
        case ButtonStateMachine::Event::LongPress:
            Scarfnet::log("Button long press!");
            onEvent(Event::eLongPress);
            break;
        case ButtonStateMachine::Event::ExtraLongPress:
            Scarfnet::log("Button extra-long press!");
            onEvent(Event::eExtraLongPress);
            break;
        case ButtonStateMachine::Event::None:
            break;
    }
}

}
