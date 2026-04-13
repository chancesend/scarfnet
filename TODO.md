# TODO

## Documentation
- [x] Add code documentation for classes and methods

## Simplification

- [x] Refactor LED setting logic of OtaManager into reusable / modular code
- [x] Split OtaManager::attemptDownload into sub-functions
- [x] Scarfnet::log should not need a line break at the end of the message
- [x] Move configuration (timings, etc) into a dedicated file

## Architecture / Reliability

- [ ] Investigate edge cases: Scarves having been running for a very long time, lots of scarves in the network, really bad network conditions, etc

## Time Sync Tuning

- [ ] Temporarily shorten heartbeat interval after a connection event to give painlessMesh more data points for time sync convergence, then restore the normal 3-second interval (e.g. 500ms for ~10 seconds after `onConnectionChange`)
- [ ] See if it helps to send out an extra sync packet after a scarf sends out a new pattern or palette

## Future Work

- [x] OTA trigger/scaffold — boot with button held for 10s enters OTA mode (yellow blink); hold again 10s → server mode (purple blink); `min_spiffs` partition table set
- [x] OTA transfer logic — receiver scans for `scarfnet-ota-v*` AP, double version check (SSID + `/info`), downloads via `HTTPClient` → `Update` library with MD5 verify; server streams running partition via `esp_partition_read()` over WPA2 AP + HTTP Basic Auth
- [ ] Add a couple of modes that attempt to do "swarming" logic by taking into account time-of-arrival delays between scarves, so that the LED patterns are not all the same (they are either offset in time, or do slightly different patterns for interesting visual effects)
- 

## Unit Tests

- [ ] Test `Scarf::onReceivedData` sync logic — extract `changeIndex`/rollover acceptance logic into a testable free function or struct (currently private and hardware-coupled)
- [ ] Add tests for `ObservableButton` state machine — press → long press → extra-long press transition sequence
