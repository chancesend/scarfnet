#include "OtaManager.h"
#include "version.h"
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

namespace Scarfnet
{

static const CRGB kReceiverColor {255, 200,   0}; // yellow
static const CRGB kServerColor   {160,   0, 160}; // purple

static const char* const kOtaSsidPrefix = "scarfnet-ota-v";
static const char* const kOtaServerIp   = "192.168.4.1";
static const uint16_t    kOtaHttpPort   = 80;

static const uint32_t kScanIntervalMs   = 8000;
static const size_t   kFlashChunkBytes  = 512;

// ─── Construction ────────────────────────────────────────────────────────────

OtaManager::OtaManager(int buttonPin, LedSetter ledSetter)
    : _buttonPin(buttonPin), _ledSetter(std::move(ledSetter))
{
}

OtaManager::~OtaManager() = default;

// ─── Boot trigger ─────────────────────────────────────────────────────────────

bool OtaManager::checkBootTrigger()
{
    pinMode(_buttonPin, INPUT);
    if (digitalRead(_buttonPin) != LOW)
        return false;

    Scarfnet::log("[OTA] Button held at boot — waiting 10s for OTA trigger...\n");

    const int kOtaHoldMs        = 10000;
    const int kBlinkHalfPeriodMs = 200;

    for (int elapsed = 0; elapsed < kOtaHoldMs; elapsed += kBlinkHalfPeriodMs)
    {
        bool on = (elapsed / kBlinkHalfPeriodMs) % 2 == 0;
        _ledSetter(on ? kReceiverColor : CRGB::Black);
        delay(kBlinkHalfPeriodMs);

        if (digitalRead(_buttonPin) != LOW)
        {
            Scarfnet::log("[OTA] Button released — skipping OTA mode\n");
            _ledSetter(CRGB::Black);
            return false;
        }
    }

    Scarfnet::log("[OTA] Entering OTA mode (firmware v%d) — receiver ready\n", FIRMWARE_VERSION);
    _active = true;
    WiFi.mode(WIFI_STA);
    return true;
}

// ─── Main loop ───────────────────────────────────────────────────────────────

void OtaManager::loop()
{
    uint32_t now = millis();

    // LED blink (suppressed during blocking receiver operations — they drive
    // the LED directly and resume the idle blink on return)
    const uint32_t kBlinkHalfPeriodMs = 500;
    if (now - _lastToggleMs >= kBlinkHalfPeriodMs)
    {
        _lastToggleMs = now;
        _ledOn = !_ledOn;
        CRGB color = (_mode == Mode::eServer) ? kServerColor : kReceiverColor;
        _ledSetter(_ledOn ? color : CRGB::Black);
    }

    bool pressed = (digitalRead(_buttonPin) == LOW);

    // Server-mode trigger: hold button 10 s while in receiver mode.
    // Require a release first so the boot-hold doesn't count.
    if (_mode == Mode::eReceiver)
    {
        if (!pressed)
        {
            _seenRelease     = true;
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

    if (_mode == Mode::eServer)
        serverLoop();
    else if (!pressed) // Only run receiver loop when button not held, since it can be blocking and the button is our only exit from it.
    {
        receiverLoop();
    }
}

// ─── Server mode ─────────────────────────────────────────────────────────────

bool OtaManager::computeFirmwareInfo()
{
    _firmwareSize = ESP.getSketchSize();
    if (_firmwareSize == 0)
    {
        Scarfnet::log("[OTA] getSketchSize() returned 0\n");
        return false;
    }

    const esp_partition_t* partition = esp_ota_get_running_partition();
    if (!partition)
    {
        Scarfnet::log("[OTA] esp_ota_get_running_partition() failed\n");
        return false;
    }

    MD5Builder md5;
    md5.begin();

    uint8_t buf[kFlashChunkBytes];
    size_t remaining = _firmwareSize;
    size_t offset    = 0;
    while (remaining > 0)
    {
        size_t chunk = (remaining < kFlashChunkBytes) ? remaining : kFlashChunkBytes;
        esp_partition_read(partition, offset, buf, chunk);
        md5.add(buf, chunk);
        offset    += chunk;
        remaining -= chunk;
    }
    md5.calculate();
    _firmwareMd5 = md5.toString();

    Scarfnet::log("[OTA] Firmware: %u bytes  MD5: %s\n", _firmwareSize, _firmwareMd5.c_str());
    return true;
}

void OtaManager::enterServerMode()
{
    Scarfnet::log("[OTA] Entering server mode (firmware v%d)\n", FIRMWARE_VERSION);
    _mode = Mode::eServer;

    if (!computeFirmwareInfo())
    {
        Scarfnet::log("[OTA] Cannot enter server mode: firmware info unavailable\n");
        _mode = Mode::eReceiver;
        return;
    }

    String ssid = String(kOtaSsidPrefix) + FIRMWARE_VERSION;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid.c_str(), kMeshPassword.c_str());
    Scarfnet::log("[OTA] AP: %s  IP: %s\n", ssid.c_str(), WiFi.softAPIP().toString().c_str());

    _webServer.reset(new WebServer(kOtaHttpPort));
    _webServer->on("/info",     HTTP_GET, [this]() { handleInfoRequest();     });
    _webServer->on("/firmware", HTTP_GET, [this]() { handleFirmwareRequest(); });
    _webServer->onNotFound([this]() { _webServer->send(404, "text/plain", "Not found"); });
    _webServer->begin();
    Scarfnet::log("[OTA] HTTP server ready on port %d\n", kOtaHttpPort);
}

void OtaManager::handleInfoRequest()
{
    if (!_webServer->authenticate(OTA_HTTP_USER, kMeshPassword.c_str()))
    {
        _webServer->requestAuthentication();
        return;
    }

    JsonDocument doc;
    doc["version"] = FIRMWARE_VERSION;
    doc["size"]    = _firmwareSize;
    doc["md5"]     = _firmwareMd5;

    String body;
    serializeJson(doc, body);
    _webServer->send(200, "application/json", body);
    Scarfnet::log("[OTA] Served /info\n");
}

void OtaManager::handleFirmwareRequest()
{
    if (!_webServer->authenticate(OTA_HTTP_USER, kMeshPassword.c_str()))
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

    Scarfnet::log("[OTA] Streaming %u bytes to client\n", _firmwareSize);

    // Write the HTTP response manually so we can stream the binary body
    // directly from flash without buffering the whole image in RAM.
    WiFiClient client = _webServer->client();
    client.setNoDelay(true);

    String headers =
        String("HTTP/1.1 200 OK\r\n") +
        "Content-Type: application/octet-stream\r\n" +
        "Content-Length: " + _firmwareSize + "\r\n" +
        "X-MD5: " + _firmwareMd5 + "\r\n" +
        "Connection: close\r\n\r\n";
    client.print(headers);

    uint8_t buf[kFlashChunkBytes];
    size_t remaining = _firmwareSize;
    size_t offset    = 0;
    while (remaining > 0 && client.connected())
    {
        size_t chunk = (remaining < kFlashChunkBytes) ? remaining : kFlashChunkBytes;
        esp_partition_read(partition, offset, buf, chunk);
        client.write(buf, chunk);
        offset    += chunk;
        remaining -= chunk;
    }

    Scarfnet::log("[OTA] Firmware stream %s (%u bytes sent)\n",
        (remaining == 0) ? "complete" : "interrupted", offset);
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
    if (now - _lastScanMs < kScanIntervalMs)
        return;
    _lastScanMs = now;

    // Orange LED during scan so it's visually distinct from the idle yellow blink.
    _ledSetter(CRGB(255, 80, 0));
    Scarfnet::log("[OTA] Scanning for OTA server AP...\n");
    int found = WiFi.scanNetworks(); // synchronous; blocks ~2 s
    _ledSetter(CRGB::Black);

    for (int i = 0; i < found; i++)
    {
        String ssid = WiFi.SSID(i);
        if (!ssid.startsWith(kOtaSsidPrefix))
            continue;

        // First version check: parse version from SSID before connecting.
        int serverVersion = ssid.substring(strlen(kOtaSsidPrefix)).toInt();
        if (serverVersion <= FIRMWARE_VERSION)
        {
            Scarfnet::log("[OTA] Found %s (v%d) — not newer than own v%d, skipping\n",
                ssid.c_str(), serverVersion, FIRMWARE_VERSION);
            continue;
        }

        Scarfnet::log("[OTA] Found %s (v%d > own v%d) — attempting download\n",
            ssid.c_str(), serverVersion, FIRMWARE_VERSION);

        WiFi.scanDelete();
        attemptDownload(ssid); // calls ESP.restart() on success; returns on failure
        return;
    }

    WiFi.scanDelete();
}

bool OtaManager::attemptDownload(const String& serverSsid)
{
    // ── Server detected: quick white strobe ──────────────────────────────────
    for (int i = 0; i < 3; i++)
    {
        _ledSetter(CRGB::White);
        delay(80);
        _ledSetter(CRGB::Black);
        delay(80);
    }

    // ── Connect ──────────────────────────────────────────────────────────────
    Scarfnet::log("[OTA] Connecting to %s...\n", serverSsid.c_str());
    WiFi.begin(serverSsid.c_str(), kMeshPassword.c_str());

    const uint32_t kConnectTimeoutMs = 15000;
    uint32_t connectStart = millis();
    uint32_t ledToggle    = millis();
    bool     ledOn        = false;

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - connectStart > kConnectTimeoutMs)
        {
            Scarfnet::log("[OTA] Connect timeout\n");
            _ledSetter(CRGB::Black);
            WiFi.disconnect();
            return false;
        }
        // Fast white blink while connecting
        if (millis() - ledToggle >= 150)
        {
            ledOn     = !ledOn;
            ledToggle = millis();
            _ledSetter(ledOn ? CRGB::White : CRGB::Black);
        }
        delay(10);
    }
    Scarfnet::log("[OTA] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

    // ── Fetch /info ───────────────────────────────────────────────────────────
    HTTPClient http;
    String base = String("http://") + kOtaServerIp;

    http.begin(base + "/info");
    http.setAuthorization(OTA_HTTP_USER, kMeshPassword.c_str());
    int code = http.GET();
    if (code != 200)
    {
        Scarfnet::log("[OTA] /info failed: HTTP %d\n", code);
        http.end();
        _ledSetter(CRGB::Black);
        WiFi.disconnect();
        return false;
    }

    JsonDocument infoDoc;
    auto err = deserializeJson(infoDoc, http.getStream());
    http.end();
    if (err)
    {
        Scarfnet::log("[OTA] /info JSON parse error: %s\n", err.c_str());
        _ledSetter(CRGB::Black);
        WiFi.disconnect();
        return false;
    }

    int    serverVersion = infoDoc["version"];
    size_t firmwareSize  = infoDoc["size"];
    String firmwareMd5   = infoDoc["md5"].as<String>();

    // Second version check: verify the /info payload, not just the SSID.
    if (serverVersion <= FIRMWARE_VERSION)
    {
        Scarfnet::log("[OTA] /info reports v%d — not newer than own v%d, aborting\n",
            serverVersion, FIRMWARE_VERSION);
        _ledSetter(CRGB::Black);
        WiFi.disconnect();
        return false;
    }
    Scarfnet::log("[OTA] Server v%d  size=%u  MD5=%s\n",
        serverVersion, firmwareSize, firmwareMd5.c_str());

    // ── Connected + info verified: white strobe confirmation ─────────────────
    for (int i = 0; i < 4; i++)
    {
        _ledSetter(CRGB::White);
        delay(60);
        _ledSetter(CRGB::Black);
        delay(60);
    }

    // ── Download /firmware ────────────────────────────────────────────────────
    http.begin(base + "/firmware");
    http.setAuthorization(OTA_HTTP_USER, kMeshPassword.c_str());
    code = http.GET();
    if (code != 200)
    {
        Scarfnet::log("[OTA] /firmware failed: HTTP %d\n", code);
        http.end();
        _ledSetter(CRGB::Black);
        WiFi.disconnect();
        return false;
    }

    if (!Update.begin(firmwareSize))
    {
        Scarfnet::log("[OTA] Update.begin failed: %s\n", Update.errorString());
        http.end();
        _ledSetter(CRGB::Black);
        WiFi.disconnect();
        return false;
    }
    Update.setMD5(firmwareMd5.c_str());

    // Manual chunk loop so we can drive the LED as progress increases.
    // Half-period interpolates from 1000 ms (0%) → 50 ms (100%).
    WiFiClient* stream = http.getStreamPtr();
    stream->setTimeout(10000);

    uint8_t  buf[kFlashChunkBytes];
    size_t   downloaded  = 0;
    uint32_t lastToggle  = millis();
    bool     pulseLedOn  = true;
    uint8_t  lastLogPct  = 0;
    _ledSetter(CRGB::Red); // start with red on

    while (downloaded < firmwareSize)
    {
        size_t toRead    = firmwareSize - downloaded;
        if (toRead > kFlashChunkBytes) toRead = kFlashChunkBytes;

        size_t bytesRead = stream->readBytes(buf, toRead);
        if (bytesRead == 0)
        {
            Scarfnet::log("[OTA] Stream read timeout at %u bytes\n", downloaded);
            break;
        }

        if (Update.write(buf, bytesRead) != bytesRead)
        {
            Scarfnet::log("[OTA] Update.write error at %u bytes\n", downloaded);
            break;
        }
        downloaded += bytesRead;

        // Log progress at every 10%
        uint8_t pct = (uint8_t)(100u * downloaded / firmwareSize);
        if (pct / 10 > lastLogPct / 10)
        {
            Scarfnet::log("[OTA] %u%%  (%u / %u bytes)\n", pct, downloaded, firmwareSize);
            lastLogPct = pct;
        }

        // Red pulse: half-period shrinks from 1000 ms → 50 ms as progress grows.
        float    progress      = (float)downloaded / (float)firmwareSize;
        uint32_t halfPeriodMs  = (uint32_t)(1000.0f - 950.0f * progress);
        uint32_t now           = millis();
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
        Scarfnet::log("[OTA] Download incomplete: got %u of %u bytes\n", downloaded, firmwareSize);
        Update.abort();
        _ledSetter(CRGB::Black);
        WiFi.disconnect();
        return false;
    }

    // Update.end() verifies MD5 and commits the new partition.
    if (!Update.end())
    {
        Scarfnet::log("[OTA] Verification failed: %s\n", Update.errorString());
        _ledSetter(CRGB::Black);
        WiFi.disconnect();
        return false;
    }

    // ── Success ───────────────────────────────────────────────────────────────
    Scarfnet::log("[OTA] Update verified — solid green, then restarting with v%d\n", serverVersion);
    _ledSetter(CRGB(0, 255, 0));
    delay(2000);
    ESP.restart();
    return true; // unreachable
}

} // namespace Scarfnet
