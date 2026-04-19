#include "ObservableButton.h"
#include "config.h"
#include "log.h"

namespace Scarfnet
{

#if SCARFNET_EMBEDDED
ObservableButton::ObservableButton(Scheduler* scheduler, uint8_t buttonPin)
    : _scheduler(scheduler),
      _button(buttonPin, 1, 10),
      _pollFn([this]() -> ButtonReading {
          // read() MUST be called first to latch the current pin state.
          // It is baked into this lambda so it cannot be accidentally omitted.
          _button.read();
          return ButtonReading{
              _button.wasPressed(),
              _button.pressedFor(kLongPressMs),
              _button.pressedFor(kExtraLongPressMs),
              _button.wasReleased()
          };
      }),
      _taskCheckButtonEvent(kButtonPollIntervalMs, TASK_FOREVER,
                            [this]() { this->checkButtonEvent(); })
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
    _scheduler->addTask(_taskCheckButtonEvent);
    _taskCheckButtonEvent.enable();
}

void ObservableButton::checkButtonEvent()
{
    const ButtonReading reading = _pollFn();

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
