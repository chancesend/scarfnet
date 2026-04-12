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

    enum class Mode
    {
        eReceiver,  // waiting for an OTA server to appear (yellow blink)
        eServer,    // serving firmware to peers (purple blink)
    };

    OtaManager(int buttonPin, LedSetter ledSetter);

    // Called once at boot (before mesh init). Checks whether the button is held
    // and counts down 10 seconds with a yellow blink. Returns true if OTA mode
    // was triggered and sets isActive() = true; returns false if the button was
    // released early.
    bool checkBootTrigger();

    // Service the OTA mode each loop iteration (LED blink, button polling,
    // future transport).
    void loop();

    bool isActive() const { return _active; }
    Mode getMode() const  { return _mode; }

private:
    void enterServerMode();

    int         _buttonPin;
    LedSetter   _ledSetter;
    bool        _active {false};
    Mode        _mode   {Mode::eReceiver};

    // LED blink state
    uint32_t    _lastToggleMs {0};
    bool        _ledOn {false};

    // Server-mode trigger: require button release after boot-hold before
    // counting a new hold, to avoid immediately entering server mode.
    bool        _seenRelease {false};
    uint32_t    _buttonHoldStartMs {0};
};

} // namespace Scarfnet
