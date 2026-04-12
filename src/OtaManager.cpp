#include "OtaManager.h"
#include "version.h"
#include "log.h"

#include <Arduino.h>

namespace Scarfnet
{

static const CRGB kReceiverColor {255, 200, 0};  // yellow
static const CRGB kServerColor   {160,   0, 160}; // purple

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
        _ledSetter(on ? kReceiverColor : CRGB::Black);
        delay(kBlinkHalfPeriodMs);

        if (digitalRead(_buttonPin) != LOW)
        {
            Scarfnet::log("Button released — skipping OTA mode\n");
            _ledSetter(CRGB::Black);
            return false;
        }
    }

    Scarfnet::log("Entering OTA mode (firmware v%d) — receiver ready\n", FIRMWARE_VERSION);
    _active = true;
    // _seenRelease stays false: loop() will wait for the boot-hold to be released
    // before it starts counting a new hold for server-mode entry.
    return true;
}

void OtaManager::enterServerMode()
{
    Scarfnet::log("Entering OTA server mode (firmware v%d)\n", FIRMWARE_VERSION);
    _mode = Mode::eServer;
    // TODO: WiFi.softAP("scarfnet-ota-v{VERSION}", kMeshPassword)
    // TODO: Start WebServer on port 80:
    //   GET /info  → JSON {version, size, md5}  (requires Basic Auth)
    //   GET /firmware → stream running partition via esp_partition_read()  (requires Basic Auth)
    // All endpoints: authenticate(OTA_HTTP_USER, kMeshPassword.c_str()) or send 401.
}

void OtaManager::loop()
{
    uint32_t now = millis();

    // --- LED blink ---
    const uint32_t kBlinkHalfPeriodMs = 500;
    if (now - _lastToggleMs >= kBlinkHalfPeriodMs)
    {
        _lastToggleMs = now;
        _ledOn = !_ledOn;
        CRGB color = (_mode == Mode::eServer) ? kServerColor : kReceiverColor;
        _ledSetter(_ledOn ? color : CRGB::Black);
    }

    // --- Server-mode trigger: hold button 10s in receiver mode ---
    // Require a release after the boot-hold before counting a new hold.
    if (_mode == Mode::eReceiver)
    {
        bool pressed = (digitalRead(_buttonPin) == LOW);

        if (!pressed)
        {
            _seenRelease = true;
            _buttonHoldStartMs = 0;
        }
        else if (_seenRelease)
        {
            if (_buttonHoldStartMs == 0)
                _buttonHoldStartMs = now;
            else if (now - _buttonHoldStartMs >= 10000)
            {
                enterServerMode();
                _buttonHoldStartMs = 0;
            }
        }
    }

    // TODO: service OTA transport
    //
    // Receiver path:
    //   - Periodically WiFi.scanNetworks() for SSIDs matching "scarfnet-ota-v*"
    //   - Parse version from SSID; skip if parsed_version <= FIRMWARE_VERSION (no rollback)
    //   - Connect with kMeshPassword; GET /info with Basic Auth (OTA_HTTP_USER / kMeshPassword)
    //   - Parse /info JSON; SECOND version check: abort if info.version <= FIRMWARE_VERSION
    //   - GET /firmware with same auth; feed chunks to Update library
    //   - Update.end() verifies MD5 — restart on pass, log + retry on fail
    //
    // Server path:
    //   - webServer.handleClient() to service incoming connections
}

} // namespace Scarfnet
