#pragma once

#include <FastLED.h>
#include <functional>
#include <memory>
#include <stdint.h>

class WebServer; // forward-declared to avoid pulling it into every file that includes OtaManager.h

namespace Scarfnet
{

// Manages OTA mode detection and the OTA update lifecycle.
// The caller owns FastLED setup and passes a LedSetter lambda so OtaManager
// can control the builtin LED without knowing the strip type or pin.
//
// Modes:
//   Receiver (yellow blink): default on boot-trigger entry. Periodically scans
//     for a server AP and downloads firmware if a newer version is found.
//   Server (purple blink): entered by holding the button 10 s while in receiver
//     mode. Starts a WiFi AP and HTTP server that serves the running firmware
//     binary to receiver scarves.
class OtaManager
{
public:
    typedef std::function<void(CRGB)> LedSetter;

    enum class Mode { eReceiver, eServer };

    OtaManager(int buttonPin, LedSetter ledSetter);
    ~OtaManager(); // defined in .cpp where WebServer is complete

    // Called once at boot (before mesh init). Returns true if OTA mode was
    // triggered (button held 10 s); false if button was released early.
    bool checkBootTrigger();

    // Must be called every loop iteration while in OTA mode (normal mesh loop is skipped).
    void loop();

    // Returns true after checkBootTrigger() confirmed OTA mode.
    bool isActive() const { return _active; }
    // Returns the current OTA sub-mode (receiver or server).
    Mode getMode()  const { return _mode; }

private:
    // --- LED helpers ---
    void strobe(CRGB color, int count, int onMs, int offMs);

    // --- Server ---
    void enterServerMode();
    bool computeFirmwareInfo(); // populates _firmwareSize and _firmwareMd5
    void handleInfoRequest();
    void handleFirmwareRequest();
    void serverLoop();

    // --- Receiver ---
    void receiverLoop();
    bool attemptDownload(const String& serverSsid);
    bool connectToServer(const String& ssid);
    bool fetchFirmwareInfo(const String& base, int& serverVersion, size_t& firmwareSize, String& firmwareMd5);
    bool downloadAndFlash(const String& base, size_t firmwareSize, const String& firmwareMd5, int serverVersion);

    int         _buttonPin;
    LedSetter   _ledSetter;
    bool        _active {false};
    Mode        _mode   {Mode::eReceiver};

    // LED blink
    uint32_t    _lastToggleMs {0};
    bool        _ledOn {false};

    // Server-mode trigger: require button release after boot-hold before
    // counting a new hold, so keeping it held at boot doesn't skip straight
    // to server mode.
    bool        _seenRelease {false};
    uint32_t    _buttonHoldStartMs {0};

    // Server state (allocated on enterServerMode())
    std::unique_ptr<WebServer> _webServer;
    size_t      _firmwareSize {0};
    String      _firmwareMd5;

    // Receiver state
    uint32_t    _lastScanMs {0};
};

} // namespace Scarfnet
