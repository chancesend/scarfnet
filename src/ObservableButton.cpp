#include "ObservableButton.h"
#include "config.h"
#include "log.h"

namespace Scarfnet
{

#if SCARFNET_EMBEDDED
ObservableButton::ObservableButton(Scheduler* scheduler, uint8_t buttonPin)
    : _scheduler(scheduler),
      _pollFn([this]() -> ButtonReading {
          return _debouncer.sample(digitalRead(_buttonPin) == LOW, millis());
      }),
      _taskCheckButtonEvent(kButtonPollIntervalMs, TASK_FOREVER,
                            [this]() { this->checkButtonEvent(); }),
      _buttonPin(buttonPin)
{
    Scarfnet::log("ObservableButton::ObservableButton()");
}
#endif

#if !SCARFNET_EMBEDDED
ObservableButton::ObservableButton(Scheduler* scheduler, PollFn pollFn)
    : _scheduler(scheduler),
      _pollFn(std::move(pollFn)),
      _taskCheckButtonEvent(kButtonPollIntervalMs, TASK_FOREVER,
                            [this]() { this->checkButtonEvent(); })
{
}
#endif

void ObservableButton::setup()
{
#if SCARFNET_EMBEDDED
    pinMode(_buttonPin, INPUT);
#endif
    _scheduler->addTask(_taskCheckButtonEvent);
    _taskCheckButtonEvent.enable();
}

void ObservableButton::checkButtonEvent()
{
    const ButtonReading reading = _pollFn();

    // Fire eDown on the press edge so callers that need the down timestamp
    // (e.g. tap-tempo) don't have to wait for the release.
    if (reading.wasJustPressed) {
        onEvent(Event::eDown);
    }

    const auto smEvent = _sm.update(reading);

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
