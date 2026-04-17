# TODO

## Documentation
- [x] Add code documentation for classes and methods

## Simplification

- [x] Refactor LED setting logic of OtaManager into reusable / modular code
- [x] Split OtaManager::attemptDownload into sub-functions
- [x] Scarfnet::log should not need a line break at the end of the message
- [x] Move configuration (timings, etc) into a dedicated file

## Architecture / Reliability

- [x] Investigate edge cases: Scarves having been running for a very long time, lots of scarves in the network, really bad network conditions, etc
- [ ] Investigate the logs for issues
    - [ ] Crashes
    - [ ] Disconnections
    - [ ] Time syncing
    - [ ] Network traffic

## Time Sync Tuning

- [x] Temporarily shorten heartbeat interval after a connection event to give painlessMesh more data points for time sync convergence, then restore the normal 3-second interval (e.g. 500ms for ~10 seconds after `onConnectionChange`)
- [x] See if it helps to send out an extra sync packet after a scarf sends out a new pattern or palette

## Future Work

- [x] OTA trigger/scaffold — boot with button held for 10s enters OTA mode (yellow blink); hold again 10s → server mode (purple blink); `min_spiffs` partition table set
- [x] OTA transfer logic — receiver scans for `scarfnet-ota-v*` AP, double version check (SSID + `/info`), downloads via `HTTPClient` → `Update` library with MD5 verify; server streams running partition via `esp_partition_read()` over WPA2 AP + HTTP Basic Auth
- [ ] Add a couple of modes that attempt to do "swarming" logic by taking into account time-of-arrival delays between scarves, so that the LED patterns are not all the same (they are either offset in time, or do slightly different patterns for interesting visual effects)
    - [ ] Add very slight delay of secondary nodes when playing received pattern?
    - [ ] Can we make all the scarves extensions of the same pattern?
- [ ] More patterns! At least 8
    * Patterns should be based more on common mesh time, so we see sync
- [x] Add timestamps to log lines


* Scarf sometimes crashes (LEDs still lit, no movement)
* Visual notification when scarf enters/leaves network?

## Bugs (from 2026-04-15 log analysis)

- [x] **[CRITICAL] Heap use-after-free crash in painlessMesh on node drop** — `Core 1 panic'ed (LoadProhibited)` at `EXCVADDR: 0xbaad567c`. `BufferedConnection` destroys its `Task` value members mid-scheduler-iteration, dangling the `nextTask` pointer in `TaskScheduler::execute()`. **Fixed**: upstream TaskScheduler (master / forthcoming v4.0.5) guards `deleteTask()` — if `iNextExecute == &aTask`, it advances the pointer before unlinking. `platformio.ini` pins to GitHub master; switch to `TaskScheduler @ ^4.0.5` once tagged. See [detailed analysis](docs/crash-analysis-painlessmesh-uaf.md).
- [x] **[HIGH] SWARM EMA poisoned by bad first delta after reconnect** — `recordArrivalDelta` seeds the EMA directly with the first raw sample. After a crash/rejoin, the first sample can be ~30 minutes off, corrupting the smoothed value for minutes. See [detailed analysis](docs/bug-swarm-ema-poisoning.md).
- [ ] **[MEDIUM] Multi-second time offset spikes during new-node join** — Offsets of 3–5 seconds visible immediately after a node joins; post-crash rejoin can show overflow values (~910s). SWARM deltas should be suppressed until the clock settles. See [detailed analysis](docs/bug-time-offset-spikes.md).
- [x] **[LOW] Duplicate node IDs in connection list** — Same node appears twice in `getNodeList()` during topology churn; causes `getNumNodes()` to over-count and blink LED wrong number of times. Fix: deduplicate in `Mesh::getNumNodes()`. See [detailed analysis](docs/bug-duplicate-node-ids.md).

## Log Visualizations

- [x] Chart 1: Time sync offset over time — `[MESH] Adjusted time` offset values as a time-series line
- [x] Chart 2: SWARM arrival deltas per node — raw (dashed) and smoothed (solid) per peer node
- [x] Chart 3: Connection events timeline — NEW / DROP / CHANGED events as a swimlane strip
- [x] Chart 4: Large time-sync offset spikes — same data as Chart 1, highlighted when |offset| > 100ms
- [x] Chart 5: Sent heartbeats — `[SND]` events with pattern/randomizer annotations and gap detection

## Investigations

- [x] See what stability we see regarding time deltas between scarves - do they drift with space? What sort of clock jitter do we have? See [investigation](docs/investigation-clock-jitter.md).
- [x] What sort of network traffic are we dealing with, relative to what is being sent? How does this scale with the number of scarves? Is there a point at which we will start seeing problems? I currently have 16 scarves that could be active at any one time - how large could this scale? See [investigation](docs/investigation-network-traffic.md).
- [x] Channel contention and topology instability — hop count in the 16-node network, and how to reduce it. See [investigation](docs/investigation-channel-contention-hops.md).

## Unit Tests

- [x] Test `Scarf::onReceivedData` sync logic — extract `changeIndex`/rollover acceptance logic into a testable free function or struct (currently private and hardware-coupled)
- [ ] Add tests for `ObservableButton` state machine — press → long press → extra-long press transition sequence
