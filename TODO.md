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
- [ ] What sort of security can we put in place to make sure that scarves only listen to other scarves? (networkID/password?, or a checksum of some sort?)

## Pattern enhancements
- [ ] Add a couple of modes that attempt to do "swarming" logic by taking into account time-of-arrival delays between scarves, so that the LED patterns are not all the same (they are either offset in time, or do slightly different patterns for interesting visual effects)
    - [ ] Add very slight delay of secondary nodes when playing received pattern?
    - [ ] Can we make all the scarves extensions of the same pattern?

## Party mode (tap-tempo) ideas
- [x] For tap-tempo mode, we should take our tempo taps on the press of the button, not the release
- [x] All modes should have some sort of party mode variant


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
- [ ] Figure out if there's a better way to form the network ad-hoc via button presses
- [x] Is there a way we can visualize the patterns natively? Figure out a way to simulate N scarves, so that we can simulate how different patterns will look in the real scarves
