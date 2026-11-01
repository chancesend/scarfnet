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
//     Also entered automatically after a normal-mode scarf finds a newer
//     server (see autoUpdateTick); then it gives up after kOtaAutoTimeoutMs.
//   Server (purple blink): entered by holding the button kOtaHoldMs while in receiver
//     mode. Starts a WiFi AP on the ESP-NOW channel and an HTTP server that serves the
//     running firmware binary to receiver scarves.
class OtaManager
{
public:
    typedef std::function<void(CRGB)> LedSetter;

    enum class Mode { eReceiver, eServer };

    OtaManager(int buttonPin, LedSetter ledSetter);
    ~OtaManager(); // defined in .cpp where WebServer is complete

    // Called once at boot, before mesh init. Returns true if OTA mode was
    // triggered by the button hold or a pending auto-update.
    bool checkBootTrigger();

    // Must be called every loop iteration while in OTA mode (normal mesh loop is skipped).
    void loop();

    // Call every loop iteration in normal mode. While a peer on newer firmware
    // has been seen, periodically runs a non-blocking scan of the ESP-NOW
    // channel for an OTA server; if one is newer than us, restarts into
    // receiver mode to update (giving up after kOtaAutoTimeoutMs).
    void autoUpdateTick(bool newerPeerSeen);

    bool isActive() const { return _active; }

private:
    // --- LED helpers ---
    void strobe(CRGB color, int count, uint32_t halfPeriodMs);

    // --- Server ---
    void enterServerMode();
    bool computeFirmwareInfo();
    void handleInfoRequest();
    void handleFirmwareRequest();
    void serverLoop();

    // --- Receiver ---
    bool fail();  // LED off, disconnect, return false
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
    bool        _autoMode    {false};  // entered via auto-update, not the button
    uint32_t    _autoStartMs {0};

    // Normal-mode auto-update scan state
    bool        _autoScanRunning {false};
    uint32_t    _lastAutoScanMs  {0};
};

} // namespace Scarfnet
