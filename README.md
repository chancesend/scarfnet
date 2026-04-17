# Scarfnet

ESP32-based (M5Stack Atom Lite) firmware that coordinates LED patterns across a mesh network of wearable scarves. Each scarf node runs the same firmware and uses painlessMesh (WiFi mesh) to synchronize button presses and LED animations in real time.

## Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- [uv](https://docs.astral.sh/uv/) for Python tooling

## Getting Started

Create `include/login.h` (gitignored) with your mesh credentials:

```cpp
#pragma once
const char* kMeshSSID     = "your-mesh-ssid";
const char* kMeshPassword = "your-mesh-password";
const uint16_t kMeshPort  = 5555;
```

## Build & Flash

```bash
# Build for the embedded target
pio run -e m5stack-atom-lite

# Flash to device
pio run -e m5stack-atom-lite --target upload

# Monitor serial output
pio device monitor --baud 115200

# Build, flash, and monitor in one step
pio run -e m5stack-atom-lite --target upload && pio device monitor
```

## Running Tests

Tests run on the native (desktop) platform — no hardware required.

```bash
# Run all tests
pio test -e native

# Run tests with verbose output
pio test -e native -v
```

Tests live in `test/test_scarfnet/`, organized into subdirectories by component (`mesh/`, `observable_button/`, `patterns/`, `sync/`, `swarm/`). `test_main.cpp` is the entry point that calls each component's test suite.

## Architecture

The firmware uses an **observer pattern** throughout. The top-level coordinator is `Scarf` (`src/scarf.cpp` / `include/Scarf.h`), which wires together these subsystems:

### Mesh (`include/Mesh.h`, `src/Mesh.cpp`)

Wraps `painlessMesh` WiFi mesh. Exposes observer hooks for connection changes (`addConnectionObserver`) and incoming JSON messages (`addReceivedDataObserver`). Handles time synchronization with rollover protection, periodic delay calculations, and EMA-smoothed per-node arrival delta tracking for swarm pattern work.

### ObservableButton (`include/ObservableButton.h`, `src/ObservableButton.cpp`)

Polls a hardware button on a `TaskScheduler` task, classifies presses into `ePress`, `eLongPress`, `eExtraLongPress`, and `eDoublePress` events, and broadcasts to registered observers.

### PatternManager (`include/PatternManager.h`)

Owns the list of LED patterns (`PatternList`) and tracks the currently active pattern and a randomizer seed. Exposes `incrementPattern`, `samePatternDifferentRandomizer`, and `changePatternFromString` for state transitions.

### OtaManager (`include/OtaManager.h`, `src/OtaManager.cpp`)

Manages peer-to-peer OTA firmware updates over a temporary WiFi AP. Operates independently of the mesh (mesh init is skipped in OTA mode).

**OTA mode**: To enter OTA update mode, hold the button at boot for 10 s (yellow blink countdown). Releasing early cancels. After entering OTA mode, normal mesh setup is skipped entirely. You will initially be in Receiver mode.

- **Receiver** (slow yellow blink): scans for a `scarfnet-ota-v{N}` AP, connects, fetches `/info`, and downloads+flashes firmware if the server version is newer. LED feedback: orange during scan → white strobes on connect → red pulse speeding up during download → solid green on success.
- **Server** (purple blink): entered by holding the button 10 s while in receiver mode. Reads running firmware from flash, starts a WPA2 AP, and serves the binary via HTTP with Basic Auth on `/info` (JSON metadata) and `/firmware` (raw binary). Uses `esp_partition_read()` + `MD5Builder` to compute firmware size and MD5 without buffering in RAM.

**Security**: double version check (SSID name + `/info` JSON), WPA2 AP with `kMeshPassword`, HTTP Basic Auth (`OTA_HTTP_USER` / `kMeshPassword`). Receivers reject downgrades.

**Partitions**: requires `board_build.partitions = min_spiffs.csv` in `platformio.ini` for dual OTA partition layout.

### Sync Protocol

When a button is pressed, `Scarf::processEvent` updates `_lastSelfButtonPressMs`, `_changeIndex`, and the local pattern, then forces an immediate broadcast. Remote nodes receive a JSON payload `{id, lastPress, pattern, randomizer, currentTimeMs, changeIndex}`. `Scarf::onReceivedData` accepts an incoming update only if `changeIndex` is newer than local state (via `shouldAcceptUpdate` in `sync.h`), preventing oscillation and echo. A rollover guard caps values at `0x7fffffff`.

After a topology change, `_taskBurstSync` fires `kBurstSyncCount` (3) extra heartbeats at `kBurstSyncIntervalMs` (500ms) intervals to give painlessMesh more timing samples for clock convergence.

### Swarm / Arrival Delta Tracking

`Mesh::recordArrivalDelta(nodeId, rawDeltaMs)` is called on every received heartbeat with `delta = receiverTimeMs - senderTimeMs`. This estimates one-way propagation delay per node, smoothed with an EMA (α=0.4, gated until the mesh clock has settled after topology changes). Values are stored in `_nodeArrivalDeltas` and logged under the `[SWARM]` prefix — future use for phase-offsetting animation patterns.

### Button Behaviors

| Press type | Action |
|---|---|
| Short press | Advance to the next pattern |
| Long press | Re-randomize the current pattern |
| Extra-long press | Cycle LED strip type (Adafruit/Amazon) and reboot |

### LED Types

Two LED strip color orders are supported, persisted in NVS (`Preferences`):
- `kLedType_Adafruit` (GRB)
- `kLedType_Amazon` (RGB)

The type is toggled via extra-long press and survives reboots.

## Key Configuration Files

| File | Purpose |
|---|---|
| `include/login.h` | **Gitignored.** Mesh SSID, password, and port. Must be created locally. |
| `include/config.h` | All timing constants (button thresholds, LED refresh, heartbeat interval, OTA timings, etc.). Single source of truth — never hard-code timing values elsewhere. |
| `include/version.h` | `FIRMWARE_VERSION` (integer, increment before OTA flashing) and `OTA_HTTP_USER`. |
| `include/defines.h` | Pin assignments, LED count, LED type enums, `SCARFNET_EMBEDDED` compile guard. |
| `include/typedefs.h` | Core type aliases: `Leds` (`std::vector<CRGB>`) and `Rnd` (`uint8_t`). |

## Python Tooling

Scripts live in `tools/`. Use `uv` — never bare `python3`.

```bash
# Run a tool script
uv run tools/scarfnet-log-viz.py logs/session.txt -o traces.json

# Scripts are also directly executable via their uv shebang
./tools/scarfnet-log-viz.py logs/session.txt -o traces.json
```

All scripts use [PEP 723 inline script metadata](https://peps.python.org/pep-0723/) to declare their Python version and dependencies.
