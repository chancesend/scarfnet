#include "OtaManager.h"
#include "version.h"
#include "log.h"

#include <Arduino.h>

namespace Scarfnet
{

OtaManager::OtaManager(int buttonPin, LedSetter ledSetter)
    : _buttonPin(buttonPin), _ledSetter(std::move(ledSetter))
{
}

bool OtaManager::checkBootTrigger()
{
    pinMode(_buttonPin, INPUT);
    if (digitalRead(_buttonPin) != LOW)
        return false;

    Scarfnet::log("Button held at boot — waiting 10s for OTA trigger...\n");

    const int kOtaHoldMs = 10000;
    const int kBlinkHalfPeriodMs = 200;

    for (int elapsed = 0; elapsed < kOtaHoldMs; elapsed += kBlinkHalfPeriodMs)
    {
        bool on = (elapsed / kBlinkHalfPeriodMs) % 2 == 0;
        _ledSetter(on ? CRGB(255, 200, 0) : CRGB::Black); // yellow
        delay(kBlinkHalfPeriodMs);

        if (digitalRead(_buttonPin) != LOW)
        {
            Scarfnet::log("Button released — skipping OTA mode\n");
            _ledSetter(CRGB::Black);
            return false;
        }
    }

    Scarfnet::log("Entering OTA mode (firmware v%d). Awaiting update...\n", FIRMWARE_VERSION);
    _active = true;
    // TODO: initialize OTA receiver (HTTP server / painlessMesh OTA channel)
    return true;
}

void OtaManager::loop()
{
    const uint32_t kBlinkHalfPeriodMs = 500;
    uint32_t now = millis();
    if (now - _lastToggleMs >= kBlinkHalfPeriodMs)
    {
        _lastToggleMs = now;
        _ledOn = !_ledOn;
        _ledSetter(_ledOn ? CRGB(255, 200, 0) : CRGB::Black); // yellow
    }
    // TODO: service OTA transport (mesh update(), HTTP handleClient(), etc.)
}

} // namespace Scarfnet
