# TODO

## Simplification

- [x] Mesh::TimeMs should be used instead of uint32_t for timestamps
- [ ] Make `SCARFNET_EMBEDDED` a real PlatformIO build flag instead of hardcoding it in `defines.h`
    - Currently `defines.h` has `#define SCARFNET_EMBEDDED (1)` unconditionally, so any header that includes `defines.h` pulls in FastLED and can't compile natively
    - Fix: remove the hardcode from `defines.h`; add `-D SCARFNET_EMBEDDED=1` to the `build_flags` of the embedded env in `platformio.ini` only (not the native env)
    - Benefit: `ObservableButton` (and anything else that includes `defines.h`) would compile natively, unlocking integration tests for the `ObservableButton` → `ButtonStateMachine` → observer path via the injectable `PollFn` constructor

## Architecture / Reliability

- [ ] Investigate the logs for issues
    - [ ] Crashes
    - [ ] Disconnections
    - [ ] Time syncing
    - [ ] Network traffic

## Future Work

- [ ] Debug mode, to add a visual notification when scarf enters/leaves network, or other network event happens?
- [ ] What sort of security can we put in place to make sure that scarves only listen to other scarves? (networkID/password?)
- [ ] What else should go into heartbeat packet? Possibly 
    - scarf version (uint16)
- [ ] HeartbeatPacket needs to be aligned better - make sure pattern name is 4-byte aligned
- [ ] Heartbeat transmit timing should have some slop/walk, to prevent continual network conflict
- [ ] Randomizer should be more than u8. Maybe u16?

## Pattern enhancements
- [ ] Add a couple of modes that attempt to do "swarming" logic by taking into account time-of-arrival delays between scarves, so that the LED patterns are not all the same (they are either offset in time, or do slightly different patterns for interesting visual effects)
    - [ ] Add very slight delay of secondary nodes when playing received pattern?
    - [ ] Can we make all the scarves extensions of the same pattern?
- [ ] Move individual patterns into separate files for modularity
- [ ] More patterns! At least 8
    * Patterns should be based more on common mesh time, so we see sync
- [ ] Make sure that when pattern and time updates are received from other scarves, the local scarf's pattern is blended over the course of a second or two, rather than have a hard swap
- [ ] How can we do a tap-tempo to time patterns with external music?

## Log Visualizations

- [ ] Investigate neighbor routing for certain messages, to reduce traffic and hops
        NeighbourPackage pkg;
        mesh.sendPackage(&pkg);
- [ ] Investigate painlessMesh plugin system for routing different kinds of nodes
        https://alteriom.github.io/painlessMesh/#/architecture/plugin-system
- [ ] Show node joined/dropped events as spans in time on their own lane

## ESP-NOW Port (see docs/plan-espnow-port.md)
- [ ] **Phase 5 — Full fleet range test**
    - [ ] Deploy all 16 scarves in a festival-like space
    - [ ] Walk scarves apart to find range limit
    - [ ] Confirm all nodes receive broadcasts from all others
    - [ ] Evaluate whether flooding (TTL re-broadcast) is needed for range

## Investigations

- [ ] Investigate whether the "clock settling" gate is actually helping, or if it is causing more problems when there are unstable networks
- [ ] Investigate running on Zigbee (would require an upgraded ESP32)
- [ ] What refactorings would make this more extensible and modular?
