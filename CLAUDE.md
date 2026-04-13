# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Project Overview

Scarfnet is an ESP32-based (M5Stack Atom Lite) firmware that coordinates LED patterns across a mesh network of wearable scarves. Each scarf node runs the same firmware and uses painlessMesh (WiFi mesh) to synchronize button presses and LED animations in real time.

## Build & Flash Commands

```bash
# Build for the embedded target (default)
pio run -e m5stack-atom-lite

# Flash to device
pio run -e m5stack-atom-lite --target upload

# Monitor serial output
pio device monitor --baud 115200

# Build and flash in one step
pio run -e m5stack-atom-lite --target upload && pio device monitor
```

## Running Tests

Tests run on the native (desktop) platform — they do not require hardware.

```bash
# Run all tests
pio test -e native

# Run tests with verbose output
pio test -e native -v
```

Tests live in `test/test_scarfnet/` and are organized into subdirectories by component (`mesh/`, `observable_button/`, `patterns/`). `test_main.cpp` is the entry point that calls each component's test suite function.

## Architecture

The firmware uses an **observer pattern** throughout. The top-level coordinator is `Scarf` (in `src/scarf.cpp` / `include/Scarf.h`), which wires together three subsystems:

### Subsystems

- **`Mesh`** (`include/Mesh.h`, `src/Mesh.cpp`) — Wraps `painlessMesh` WiFi mesh. Exposes observer hooks for connection changes (`addConnectionObserver`) and incoming JSON messages (`addReceivedDataObserver`). Handles time synchronization with rollover protection and periodic delay calculations.

- **`ObservableButton`** (`include/ObservableButton.h`, `src/ObservableButton.cpp`) — Polls a hardware button on a `TaskScheduler` task, classifies presses into `ePress`, `eLongPress`, `eExtraLongPress`, and `eDoublePress` events, and broadcasts to registered observers.

- **`PatternManager`** (`include/PatternManager.h`) — Owns the list of LED patterns (`PatternList`) and tracks the currently active pattern and a randomizer seed. Exposes `incrementPattern`, `samePatternDifferentRandomizer`, and `changePatternFromString` for state changes.

- **`OtaManager`** (`include/OtaManager.h`, `src/OtaManager.cpp`) — Manages peer-to-peer OTA firmware updates over a temporary WiFi AP. Operates independently of the mesh (mesh init is skipped in OTA mode). Two modes:
  - **Receiver** (yellow blink): periodically scans for a `scarfnet-ota-v{N}` AP, connects, fetches `/info`, and downloads+flashes firmware if the server version is newer. LED feedback: orange during scan → white strobes on detect/connect → red pulse speeding up during download → solid green on success.
  - **Server** (purple blink): entered by holding the button 10 s while in receiver mode. Reads running firmware from flash, starts a WPA2 WiFi AP, and serves the binary via HTTP with Basic Auth on `/info` (JSON metadata) and `/firmware` (raw binary). Uses `esp_partition_read()` + `MD5Builder` to compute firmware size and MD5 without buffering in RAM.

  **Triggering OTA mode**: hold the button at boot for 10 s (yellow blink countdown). Releasing early cancels it. After entering OTA mode, normal mesh setup is skipped entirely.

  **Security**: double version check (SSID name + `/info` JSON), WPA2 AP with `kMeshPassword`, and HTTP Basic Auth (`OTA_HTTP_USER` / `kMeshPassword`). Receivers reject downgrades.

  **Partitions**: requires `board_build.partitions = min_spiffs.csv` in `platformio.ini` for dual OTA partition layout.

### Sync Protocol

When a button is pressed, `Scarf::processEvent` updates `_lastSelfButtonPressMs`, `_changeIndex`, and the local pattern, then forces an immediate broadcast via `TaskScheduler`. Remote nodes receive a JSON payload containing `{id, lastPress, pattern, randomizer, currentTimeMs, changeIndex}`. `Scarf::onReceivedData` accepts an incoming update only if `changeIndex` or `lastPress` is newer than local state, preventing oscillation and echo. A rollover guard caps values at `0x7fffffff`.

### Button Behaviors

| Press type | Action |
|---|---|
| Short press | Advance to the next pattern |
| Long press | Re-randomize the current pattern |
| Extra-long press | Cycle LED strip type (Adafruit/Amazon) and reboot |

### LED Types

Two LED strip color orders are supported and persisted in NVS (`Preferences`):
- `kLedType_Adafruit` (GRB)
- `kLedType_Amazon` (RGB)

The type is toggled via extra-long press and survives reboots.

## Key Configuration Files

- `include/login.h` — **Not in git.** Contains mesh SSID, password, and port (`kMeshSSID`, `kMeshPassword`, `kMeshPort`). Must be created locally.
- `include/config.h` — All timing constants for the whole codebase (button poll/hold thresholds, LED refresh rates, heartbeat interval, OTA scan/connect/blink timings, flash chunk size). Single source of truth — do not hard-code timing values elsewhere.
- `include/version.h` — `FIRMWARE_VERSION` (integer, increment before flashing OTA updates) and `OTA_HTTP_USER` (HTTP Basic Auth username).
- `include/defines.h` — Pin assignments, LED count, LED type enums, and the `SCARFNET_EMBEDDED` compile guard.
- `include/typedefs.h` — Core type aliases: `Leds` (`std::vector<CRGB>`) and `Rnd` (`uint8_t`).

## Compile-Time Guard

Code that requires Arduino/ESP32 hardware is wrapped in `#if SCARFNET_EMBEDDED`. This flag is set to `1` in `defines.h` for the embedded target but is absent in native test builds, allowing shared logic to compile on the host.
