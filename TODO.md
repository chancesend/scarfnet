# TODO

## Pattern enhancements
- [ ] Add a couple of modes that attempt to do "swarming" logic by taking into account time-of-arrival delays between scarves, so that the LED patterns are not all the same (they are either offset in time, or do slightly different patterns for interesting visual effects)
    - [ ] Add very slight delay of secondary nodes when playing received pattern?
    - [ ] Can we make all the scarves extensions of the same pattern?

- [ ] Refactor the patterns so that the interfaces are more modular
    - [ ] Change the debug pattern context info so that it's not so specialized

- [ ] What other interesting patterns/modes can we implement using completely unique methods of light visualizations? ie, turns out the debug pattern is super fun and interesting when you have a ton of scarves, and it operates via a completely different method than the other patterns

## Physical upgrades
- [ ] Overall LED brightness effected by flashes due to current limiting - can this be fixed with a capacitor?
- [ ] Let's just solder and heat-shrink the connector to the LED strip 

## Investigations

- [ ] Investigate running on Zigbee/Thread (would require an upgraded ESP32)
- [ ] Figure out if there's a better way to form the network ad-hoc via button presses

## Refactoring
Reliability first:
- [ ] Clock offset is a `float` (`clock_sync.h`): 1 ms resolution only up to ~4.7 h of network time, so after days of fleet uptime small corrections round away. Switch to int32 ms with an integer EMA, and add a test at offset ≈ 2.6e8. Same pass: the signed-overflow UB in `(int32_t)a - (int32_t)b` (use `(int32_t)(a - b)`)
- [ ] Auto-OTA can reboot-loop: `_newerVersionSeen` is sticky, so a download that always fails (MD5, `Update.begin`) repeats forever. Add an RTC attempt counter / backoff. Also `kOtaHttpTimeoutMs` (30 s) < `kOtaServeTimeoutMs` (60 s), contradicting its comment

Cleanup:
- [ ] `rolloverGuard` is applied to `lastPressMs` (a time), and a changeIndex past 2^31 would make every peer adopt 0 and re-accept forever. Delete it or compare wrap-safely; fold `sync.h` into `sync_state.h`
- [ ] README rewrite: still describes painlessMesh/JSON/`[SWARM]`, names `Scarf.h`, wrong default LED type, long-press row ignores tap tempo
- [ ] Heap allocation in render paths: `Leds prevLeds` per frame during crossfade (`PatternManager.cpp`), palette vector rebuilt per pattern change (`palettes.cpp`); merge the parallel palette name/palette lists
- [ ] Node-left log counts include the departing node (`NodeTracker::checkTimeouts` calls back before erase)
- [ ] Sim Makefile: wildcard the pattern sources, and depend on headers
- [ ] Consistency: pattern functions take `int32_t timeMs` (negative after 24.8 days), add `__attribute__((format(printf,1,2)))` to `log()`
- [ ] Extract `Scarf::onReceived` / local-press decisions into a hardware-free, tested `SyncController` returning actions (apply, rebroadcast, alarm, send); share the node-flash table with the sim (`node_flash.h`, key it on local millis). Enables a native N-scarf lossy-network test
- [ ] Drop the M5Stack library (only used for `Button`): smaller image, faster OTA. Needs a hardware button test
- [ ] Extract the rx ring buffer into a testable `rx_queue.h`
- [ ] Join bursts: every peer sends 3 heartbeats per join; consider having only one or two random peers respond

# Actual testing up at Burning Man
## Bugs found
- [ ] Can get into a state where 1 scarf isn't updating any of the others on button push, but is changing its own pattern (so we know it isn't crashed). This is fixed on reset of that scarf

## More things to explore
- [ ] Is there any way to test network errors and missed packets via simulation?