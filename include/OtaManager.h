#pragma once

#include <FastLED.h>
#include <functional>
#include <stdint.h>

namespace Scarfnet
{

// Manages OTA mode detection and the OTA update lifecycle.
// The caller owns FastLED setup and passes a LedSetter lambda so OtaManager
// can control the builtin LED without knowing the strip type or pin.
class OtaManager
{
public:
    // Called by the owner to set the builtin LED color and show it.
    typedef std::function<void(CRGB)> LedSetter;

    OtaManager(int buttonPin, LedSetter ledSetter);

    // Called once at boot (before mesh init). Checks whether the button is held
    // and counts down 10 seconds with a yellow blink. Returns true if OTA mode
    // was triggered and sets isActive() = true; returns false if the button was
    // released early.
    bool checkBootTrigger();

    // Service the OTA mode each loop iteration (LED blink + future transport).
    void loop();

    bool isActive() const { return _active; }

private:
    int         _buttonPin;
    LedSetter   _ledSetter;
    bool        _active {false};
    uint32_t    _lastToggleMs {0};
    bool        _ledOn {false};
};

} // namespace Scarfnet
