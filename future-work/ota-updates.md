# Over-The-Air (OTA) Firmware Updates

## Goal

Flash one scarf via USB as normal. That scarf distributes the new firmware to all other scarves in the mesh over WiFi, so no scarf ever needs to be physically connected to a computer again after the first one is updated.

---

## Prerequisites (done)

- `board_build.partitions = min_spiffs.csv` — two ~1.9 MB OTA app partitions
- `include/version.h` with `FIRMWARE_VERSION` — increment before each release build
- `OtaManager` class with boot-hold trigger and server/receiver mode state machine

---

## UX / Trigger Sequence

| Step | Action | Visual |
|---|---|---|
| Boot with button held | Hold for 10 s → OTA mode | Yellow blink |
| Release early (< 10 s) | Normal boot | — |
| In OTA mode, hold again 10 s | Becomes the firmware server | Purple blink |
| Receiver finds server | Downloads, verifies, reboots | Yellow blink during download |

The scarf that was freshly USB-flashed becomes the server. All others enter as receivers.

---

## Transport Design: Direct WiFi AP

OTA mode is entered **before** mesh init, so painlessMesh is not available for signaling. Instead, the server and receiver use raw WiFi:

**Server (purple blink):**
1. `WiFi.softAP("scarfnet-ota-v{VERSION}", kMeshPassword)` — AP is WPA2-protected with the mesh password
2. Starts a synchronous HTTP server (WebServer on port 80) at `192.168.4.1`
3. All endpoints require HTTP Basic Auth (`username: "scarfnet"`, `password: kMeshPassword`); unauthenticated requests get 401
4. Responds to `GET /info` with `{"version": N, "size": M, "md5": "..."}` so receivers can verify before committing
5. Serves `GET /firmware` — streams the running firmware binary using `esp_ota_get_running_partition()` + `esp_partition_read()`
6. Keeps serving indefinitely until reset

**Receiver (yellow blink):**
1. Every few seconds, `WiFi.scanNetworks()` looking for any SSID matching `scarfnet-ota-v*`
2. Parses version from SSID; **skips if parsed version ≤ own `FIRMWARE_VERSION`** (no rollback)
3. Connects as station using `kMeshPassword`
4. `HTTPClient` with Basic Auth (`"scarfnet"` / `kMeshPassword`): fetch `/info`, parse version + MD5
5. **Second version check against `/info` response** — abort if server version ≤ own (guards against a race where SSID was stale or version was misread from the scan)
6. Fetch `/firmware` with same auth → feed chunks to `Update` library
7. On completion: `Update.end()` checks MD5; if OK → `ESP.restart()`; if not → log error, disconnect, keep scanning

### Why not mesh?

OTA mode skips all mesh/TaskScheduler setup intentionally — if the firmware is bad enough to need an OTA, you want the update path to be as simple and independent as possible. Direct WiFi has fewer failure modes.

### Why not SPIFFS/binary storage?

The server reads firmware directly from the running OTA partition using `esp_partition_read()`. No extra storage needed. The `min_spiffs` layout still leaves a small SPIFFS area for future use.

---

## Safety Properties

| Risk | Mitigation |
|---|---|
| Power cut mid-flash | `Update` library only swaps partition pointer after MD5 passes; old firmware boots on partial write |
| Corrupted download | MD5 verified by `Update.end()` before `ESP.restart()`; scarf stays on old firmware if check fails |
| Bad firmware bricks all scarves | Manual trigger required; fix and re-flash the seed via USB |
| Server reboots / disconnects mid-transfer | Receiver catches HTTP error, logs, waits, and retries on next scan cycle |
| Multiple receivers at once | `WiFi.softAP` supports up to 4 simultaneous stations on ESP32 |
| Older server visible on scan | Version checked twice: once from SSID before connecting, once from `/info` after — receiver never downloads if server version ≤ own |
| Firmware rollback attack | Same double version check; receiver rejects any server reporting a version ≤ its own |
| Traffic snooping | WiFi AP uses WPA2 (`kMeshPassword`); HTTP endpoints require Basic Auth with same password; both layers must be broken to read the binary |

---

## Implementation Order

1. ~~Add `include/version.h` and `FIRMWARE_VERSION`~~ ✓
2. ~~Change partition scheme in `platformio.ini`~~ ✓
3. ~~`OtaManager` boot trigger + server/receiver mode UX~~ ✓
4. ~~Implement server mode — WiFi AP, WebServer `/info` + `/firmware`, `esp_partition_read()` streaming~~ ✓
5. ~~Implement receiver mode — periodic scan, double version check, `HTTPClient` + `Update` library~~ ✓
6. **Test with two scarves** before rolling out to full mesh
7. Add `version` field to normal heartbeat messages in `Scarf::sendMessage()` (for future auto-detection)

---

## Libraries Already Available

| Library | Role |
|---|---|
| `Update` | Chunked OTA write, MD5 verification, partition swap |
| `HTTPClient` | Download firmware binary from server scarf |
| `WiFi` | Soft AP (server) and station (receiver) modes |
| `WebServer` | Synchronous HTTP server for the firmware endpoint |
| `ArduinoJson` | `/info` JSON response |
| `esp_partition.h` | Read running firmware binary for streaming |
