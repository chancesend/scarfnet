#include "OtaManager.h"
#include "config.h"
#include "log.h"
#include "login.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Update.h>
#include <MD5Builder.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_attr.h>

#include "ota_ssid.h"

namespace Scarfnet
{

static const CRGB kReceiverColor {255, 200,   0}; // yellow
static const CRGB kServerColor   {160,   0, 160}; // purple

static const char* const kOtaServerIp   = "192.168.4.1";
static const uint16_t    kOtaHttpPort   = 80;

// Survives a software restart (not a power cycle): set just before restarting
// into auto-update mode, read and cleared by checkBootTrigger().
static const uint32_t kAutoOtaMagic = 0x4155544Fu;  // "AUTO"
RTC_NOINIT_ATTR static uint32_t gAutoOtaRequest;

// Shared by firmware hashing, serving and downloading, which never overlap.
// Static: too big for the loop stack.
static uint8_t sChunkBuf[kOtaFlashChunkBytes];

// ─── Construction ────────────────────────────────────────────────────────────

OtaManager::OtaManager(int buttonPin, LedSetter ledSetter)
    : _buttonPin(buttonPin), _ledSetter(std::move(ledSetter))
{
}

OtaManager::~OtaManager() = default;

// ─── LED helper ──────────────────────────────────────────────────────────────

void OtaManager::strobe(CRGB color, int count, uint32_t halfPeriodMs)
{
    for (int i = 0; i < count; i++)
    {
        _ledSetter(color);
        delay(halfPeriodMs);
        _ledSetter(CRGB::Black);
        delay(halfPeriodMs);
    }
}

bool OtaManager::fail()
{
    _ledSetter(CRGB::Black);
    WiFi.disconnect();
    return false;
}

// ─── Boot trigger ─────────────────────────────────────────────────────────────

bool OtaManager::checkBootTrigger()
{
    if (gAutoOtaRequest == kAutoOtaMagic) {
        gAutoOtaRequest = 0;  // one-shot: the next boot is normal
        Scarfnet::log("[OTA] Auto-update requested — entering receiver mode (firmware v%d)",
                      kScarfVersion);
        _active      = true;
        _autoMode    = true;
        _autoStartMs = millis();
        _lastScanMs  = millis() - kOtaScanIntervalMs;  // scan right away
        WiFi.mode(WIFI_STA);
        return true;
    }

    pinMode(_buttonPin, INPUT);
    if (digitalRead(_buttonPin) != LOW)
        return false;

    Scarfnet::log("[OTA] Button held at boot — waiting %us for OTA trigger...", (unsigned)(kOtaHoldMs / 1000));

    for (uint32_t elapsed = 0; elapsed < kOtaHoldMs; elapsed += kOtaBootBlinkHalfPeriodMs)
    {
        bool on = (elapsed / kOtaBootBlinkHalfPeriodMs) % 2 == 0;
        _ledSetter(on ? kReceiverColor : CRGB::Black);
        delay(kOtaBootBlinkHalfPeriodMs);

        if (digitalRead(_buttonPin) != LOW)
        {
            Scarfnet::log("[OTA] Button released — skipping OTA mode");
            _ledSetter(CRGB::Black);
            return false;
        }
    }

    Scarfnet::log("[OTA] Entering OTA mode (firmware v%d) — receiver ready", kScarfVersion);
    _active = true;
    WiFi.mode(WIFI_STA);
    return true;
}

// ─── Main loop ───────────────────────────────────────────────────────────────

void OtaManager::loop()
{
    uint32_t now = millis();

    // An automatic update that found nothing to install gives up and returns
    // the scarf to normal operation (the RTC flag is already cleared). Only a
    // receiver times out — if someone switched this scarf to server mode, it
    // stays a server.
    if (_autoMode && _mode == Mode::eReceiver && now - _autoStartMs >= kOtaAutoTimeoutMs) {
        Scarfnet::log("[OTA] Auto-update timed out — restarting to normal mode");
        ESP.restart();
    }

    // LED blink (pauses naturally during blocking receiver operations)
    if (now - _lastToggleMs >= kOtaIdleBlinkHalfPeriodMs)
    {
        _lastToggleMs = now;
        _ledOn = !_ledOn;
        CRGB color = (_mode == Mode::eServer) ? kServerColor : kReceiverColor;
        _ledSetter(_ledOn ? color : CRGB::Black);
    }

    bool pressed = (digitalRead(_buttonPin) == LOW);

    // Server-mode trigger: hold button kOtaHoldMs while in receiver mode.
    // Require a release first so the boot-hold doesn't count.
    if (_mode == Mode::eReceiver)
    {
        if (!pressed)
        {
            _seenRelease       = true;
            _buttonHoldStartMs = 0;
        }
        else if (_seenRelease)
        {
            if (_buttonHoldStartMs == 0)
                _buttonHoldStartMs = now;
            else if (now - _buttonHoldStartMs >= kOtaHoldMs)
            {
                enterServerMode();
                _buttonHoldStartMs = 0;
            }
        }
    }

    if (_mode == Mode::eServer)
        serverLoop();
    else if (!pressed) // skip receiver loop while button held (scan is blocking)
        receiverLoop();
}

// ─── Server mode ─────────────────────────────────────────────────────────────

bool OtaManager::computeFirmwareInfo()
{
    _firmwareSize = ESP.getSketchSize();
    if (_firmwareSize == 0)
    {
        Scarfnet::log("[OTA] getSketchSize() returned 0");
        return false;
    }

    const esp_partition_t* partition = esp_ota_get_running_partition();
    if (!partition)
    {
        Scarfnet::log("[OTA] esp_ota_get_running_partition() failed");
        return false;
    }

    MD5Builder md5;
    md5.begin();

    size_t remaining = _firmwareSize;
    size_t offset    = 0;
    while (remaining > 0)
    {
        size_t chunk = (remaining < kOtaFlashChunkBytes) ? remaining : kOtaFlashChunkBytes;
        if (esp_partition_read(partition, offset, sChunkBuf, chunk) != ESP_OK)
        {
            Scarfnet::log("[OTA] esp_partition_read() failed at %u", offset);
            return false;
        }
        md5.add(sChunkBuf, chunk);
        offset    += chunk;
        remaining -= chunk;
    }
    md5.calculate();
    _firmwareMd5 = md5.toString();

    Scarfnet::log("[OTA] Firmware: %u bytes  MD5: %s", _firmwareSize, _firmwareMd5.c_str());
    return true;
}

void OtaManager::enterServerMode()
{
    Scarfnet::log("[OTA] Entering server mode (firmware v%d)", kScarfVersion);
    _mode = Mode::eServer;

    if (!computeFirmwareInfo())
    {
        Scarfnet::log("[OTA] Cannot enter server mode: firmware info unavailable");
        _mode = Mode::eReceiver;
        return;
    }

    // AP on the ESP-NOW channel so normal-mode scarves can find it with a
    // single-channel scan without leaving the mesh (see autoUpdateTick).
    String ssid = String(kOtaSsidPrefix) + kScarfVersion;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid.c_str(), kMeshPassword.c_str(), kEspNowChannel);
    Scarfnet::log("[OTA] AP: %s  IP: %s", ssid.c_str(), WiFi.softAPIP().toString().c_str());

    _webServer.reset(new WebServer(kOtaHttpPort));
    _webServer->on("/info",     HTTP_GET, [this]() { handleInfoRequest();     });
    _webServer->on("/firmware", HTTP_GET, [this]() { handleFirmwareRequest(); });
    _webServer->onNotFound([this]() { _webServer->send(404, "text/plain", "Not found"); });
    _webServer->begin();
    Scarfnet::log("[OTA] HTTP server ready on port %d", kOtaHttpPort);
}

void OtaManager::handleInfoRequest()
{
    if (!_webServer->authenticate(kOtaHttpUser, kMeshPassword.c_str()))
    {
        _webServer->requestAuthentication();
        return;
    }

    JsonDocument doc;
    doc["version"] = kScarfVersion;
    doc["size"]    = _firmwareSize;
    doc["md5"]     = _firmwareMd5;

    String body;
    serializeJson(doc, body);
    _webServer->send(200, "application/json", body);
    Scarfnet::log("[OTA] Served /info");
}

void OtaManager::handleFirmwareRequest()
{
    if (!_webServer->authenticate(kOtaHttpUser, kMeshPassword.c_str()))
    {
        _webServer->requestAuthentication();
        return;
    }

    const esp_partition_t* partition = esp_ota_get_running_partition();
    if (!partition || _firmwareSize == 0)
    {
        _webServer->send(500, "text/plain", "Partition error");
        return;
    }

    Scarfnet::log("[OTA] Streaming %u bytes to client", _firmwareSize);

    WiFiClient client = _webServer->client();
    client.setNoDelay(true);

    String headers =
        String("HTTP/1.1 200 OK\r\n") +
        "Content-Type: application/octet-stream\r\n" +
        "Content-Length: " + _firmwareSize + "\r\n" +
        "X-MD5: " + _firmwareMd5 + "\r\n" +
        "Connection: close\r\n\r\n";
    client.print(headers);

    // A receiver that drops out mid-download can leave the socket looking
    // connected while every write blocks and fails. Stop on the first short
    // write, and cap the whole transfer, so one bad client can't hang the
    // server for everyone else.
    size_t   remaining = _firmwareSize;
    size_t   offset    = 0;
    uint32_t start     = millis();
    const char* why    = "complete";
    while (remaining > 0)
    {
        if (!client.connected())                      { why = "interrupted (client disconnected)"; break; }
        if (millis() - start >= kOtaServeTimeoutMs)   { why = "aborted (transfer timeout)";        break; }

        size_t chunk = (remaining < kOtaFlashChunkBytes) ? remaining : kOtaFlashChunkBytes;
        if (esp_partition_read(partition, offset, sChunkBuf, chunk) != ESP_OK)
                                                      { why = "aborted (flash read failed)";       break; }
        if (client.write(sChunkBuf, chunk) != chunk)  { why = "aborted (write failed)";            break; }
        offset    += chunk;
        remaining -= chunk;
    }
    if (remaining > 0) client.stop();

    Scarfnet::log("[OTA] Firmware stream %s (%u of %u bytes sent, %u ms)",
        why, offset, _firmwareSize, millis() - start);
}

void OtaManager::serverLoop()
{
    if (_webServer)
        _webServer->handleClient();
}

// ─── Receiver mode ───────────────────────────────────────────────────────────

void OtaManager::receiverLoop()
{
    uint32_t now = millis();
    if (now - _lastScanMs < kOtaScanIntervalMs)
        return;
    _lastScanMs = now;

    _ledSetter(CRGB(255, 80, 0)); // solid orange during scan
    Scarfnet::log("[OTA] Scanning for OTA server AP...");
    // An auto-started receiver knows the server is on the mesh channel, so it
    // scans just that channel (~150 ms) instead of all of them (~2 s).
    int found = _autoMode
        ? WiFi.scanNetworks(false, false, false, kOtaAutoScanDwellMs, kEspNowChannel)
        : WiFi.scanNetworks();  // synchronous; blocks ~2 s
    _ledSetter(CRGB::Black);

    for (int i = 0; i < found; i++)
    {
        String ssid = WiFi.SSID(i);
        int serverVersion = otaServerVersionFromSsid(ssid.c_str());
        if (serverVersion < 0)
            continue;

        if (serverVersion <= kScarfVersion)
        {
            Scarfnet::log("[OTA] Found %s (v%d) — not newer than own v%d, skipping",
                ssid.c_str(), serverVersion, kScarfVersion);
            continue;
        }

        Scarfnet::log("[OTA] Found %s (v%d > own v%d) — attempting download",
            ssid.c_str(), serverVersion, kScarfVersion);

        WiFi.scanDelete();
        attemptDownload(ssid);
        return;
    }

    WiFi.scanDelete();
}

bool OtaManager::attemptDownload(const String& serverSsid)
{
    strobe(CRGB::White, kOtaServerFoundStrobeCount, kOtaServerFoundStrobeHalfPeriodMs);

    if (!connectToServer(serverSsid))
        return false;

    String base = String("http://") + kOtaServerIp;

    int version; size_t size; String md5;
    if (!fetchFirmwareInfo(base, version, size, md5))
        return false;

    strobe(CRGB::White, kOtaInfoVerifiedStrobeCount, kOtaInfoVerifiedStrobeHalfPeriodMs);

    return downloadAndFlash(base, size, md5, version);
}

bool OtaManager::connectToServer(const String& ssid)
{
    Scarfnet::log("[OTA] Connecting to %s...", ssid.c_str());
    WiFi.begin(ssid.c_str(), kMeshPassword.c_str());

    uint32_t start     = millis();
    uint32_t ledToggle = millis();
    bool     ledOn     = false;

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - start > kOtaConnectTimeoutMs)
        {
            Scarfnet::log("[OTA] Connect timeout");
            return fail();
        }
        if (millis() - ledToggle >= kOtaConnectLedHalfPeriodMs)
        {
            ledOn     = !ledOn;
            ledToggle = millis();
            _ledSetter(ledOn ? CRGB::White : CRGB::Black);
        }
        delay(kOtaConnectPollMs);
    }
    Scarfnet::log("[OTA] Connected. IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

bool OtaManager::fetchFirmwareInfo(const String& base, int& serverVersion, size_t& firmwareSize, String& firmwareMd5)
{
    HTTPClient http;
    http.begin(base + "/info");
    http.setTimeout(kOtaHttpTimeoutMs);  // the server may be busy streaming to another scarf
    http.setAuthorization(kOtaHttpUser, kMeshPassword.c_str());
    int code = http.GET();
    if (code != 200)
    {
        Scarfnet::log("[OTA] /info failed: HTTP %d", code);
        http.end();
        return fail();
    }

    JsonDocument doc;
    auto err = deserializeJson(doc, http.getStream());
    http.end();
    if (err)
    {
        Scarfnet::log("[OTA] /info JSON parse error: %s", err.c_str());
        return fail();
    }

    serverVersion = doc["version"];
    firmwareSize  = doc["size"];
    firmwareMd5   = doc["md5"].as<String>();

    // Second version check: verify the payload, not just the SSID.
    if (serverVersion <= kScarfVersion)
    {
        Scarfnet::log("[OTA] /info reports v%d — not newer than own v%d, aborting",
            serverVersion, kScarfVersion);
        return fail();
    }

    Scarfnet::log("[OTA] Server v%d  size=%u  MD5=%s",
        serverVersion, firmwareSize, firmwareMd5.c_str());
    return true;
}

bool OtaManager::downloadAndFlash(const String& base, size_t firmwareSize, const String& firmwareMd5, int serverVersion)
{
    HTTPClient http;
    http.begin(base + "/firmware");
    http.setTimeout(kOtaHttpTimeoutMs);
    http.setAuthorization(kOtaHttpUser, kMeshPassword.c_str());
    int code = http.GET();
    if (code != 200)
    {
        Scarfnet::log("[OTA] /firmware failed: HTTP %d", code);
        http.end();
        return fail();
    }

    if (!Update.begin(firmwareSize))
    {
        Scarfnet::log("[OTA] Update.begin failed: %s", Update.errorString());
        http.end();
        return fail();
    }
    Update.setMD5(firmwareMd5.c_str());

    // Manual chunk loop so the LED pulse can speed up with progress.
    WiFiClient* stream = http.getStreamPtr();
    stream->setTimeout(kOtaStreamReadTimeoutMs);

    size_t   downloaded = 0;
    uint32_t lastToggle = millis();
    bool     pulseLedOn = true;
    uint8_t  lastLogPct = 0;
    _ledSetter(CRGB::Red);

    while (downloaded < firmwareSize)
    {
        size_t toRead    = firmwareSize - downloaded;
        if (toRead > kOtaFlashChunkBytes) toRead = kOtaFlashChunkBytes;

        size_t bytesRead = stream->readBytes(sChunkBuf, toRead);
        if (bytesRead == 0)
        {
            Scarfnet::log("[OTA] Stream read timeout at %u bytes", downloaded);
            break;
        }
        if (Update.write(sChunkBuf, bytesRead) != bytesRead)
        {
            Scarfnet::log("[OTA] Update.write error at %u bytes", downloaded);
            break;
        }
        downloaded += bytesRead;

        uint8_t pct = (uint8_t)(100u * downloaded / firmwareSize);
        if (pct / 10 > lastLogPct / 10)
        {
            Scarfnet::log("[OTA] %u%%  (%u / %u bytes)", pct, downloaded, firmwareSize);
            lastLogPct = pct;
        }

        float    progress     = (float)downloaded / (float)firmwareSize;
        uint32_t halfPeriodMs = kOtaProgressPulseStartHalfPeriodMs - (uint32_t)(progress *
            (float)(kOtaProgressPulseStartHalfPeriodMs - kOtaProgressPulseEndHalfPeriodMs));
        uint32_t now          = millis();
        if (now - lastToggle >= halfPeriodMs)
        {
            pulseLedOn = !pulseLedOn;
            _ledSetter(pulseLedOn ? CRGB::Red : CRGB::Black);
            lastToggle = now;
        }
    }

    http.end();

    if (downloaded != firmwareSize)
    {
        Scarfnet::log("[OTA] Download incomplete: got %u of %u bytes", downloaded, firmwareSize);
        Update.abort();
        return fail();
    }

    if (!Update.end())
    {
        Scarfnet::log("[OTA] Verification failed: %s", Update.errorString());
        return fail();
    }

    Scarfnet::log("[OTA] Update verified — restarting with v%d", serverVersion);
    _ledSetter(CRGB(0, 255, 0));
    delay(kOtaSuccessDisplayMs);
    ESP.restart();
    return true; // unreachable
}

// ─── Automatic update (normal mode) ──────────────────────────────────────────

void OtaManager::autoUpdateTick(bool newerPeerSeen)
{
    if (!newerPeerSeen) return;

    if (_autoScanRunning) {
        int16_t found = WiFi.scanComplete();
        if (found == WIFI_SCAN_RUNNING) return;
        _autoScanRunning = false;

        int best = -1;
        for (int i = 0; i < found; i++) {
            int v = otaServerVersionFromSsid(WiFi.SSID(i).c_str());
            if (v > best) best = v;
        }
        WiFi.scanDelete();

        if (best > kScarfVersion) {
            Scarfnet::log("[OTA] Auto-update: server v%d found (own v%d) — restarting into receiver mode",
                          best, kScarfVersion);
            gAutoOtaRequest = kAutoOtaMagic;
            delay(kOtaRestartFlushMs);
            ESP.restart();
        }
        return;
    }

    uint32_t now = millis();
    if (now - _lastAutoScanMs < kOtaAutoScanIntervalMs) return;
    _lastAutoScanMs = now;

    // Async, single channel: the radio stays on the mesh channel and the loop
    // keeps running, so the scarf keeps syncing while it looks.
    int16_t r = WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false, /*passive=*/false,
                                  kOtaAutoScanDwellMs, kEspNowChannel);
    _autoScanRunning = (r == WIFI_SCAN_RUNNING);
    Scarfnet::log("[OTA] Auto-update: newer peer seen — scanning channel %u for an OTA server%s",
                  kEspNowChannel, _autoScanRunning ? "" : " (scan failed to start)");
}

} // namespace Scarfnet
