# TODO


## Pattern enhancements
- [ ] Add a couple of modes that attempt to do "swarming" logic by taking into account time-of-arrival delays between scarves, so that the LED patterns are not all the same (they are either offset in time, or do slightly different patterns for interesting visual effects)
    - [ ] Add very slight delay of secondary nodes when playing received pattern?
    - [ ] Can we make all the scarves extensions of the same pattern?
- [ ] Work on "digital" effect
- [ ] Debug mode, to add a visual notification for each heartbeat that arrives at the scarf, to help debug the mesh
- [ ] Overall LED brightness effected by flashes due to current limiting - can this be fixed? Might need a capacitor
- [ ] Fix up the simulator, make sure it doesn't break again (via unit test)


## ESP-NOW Port (see docs/plan-espnow-port.md)
- [ ] **Phase 5 — Full fleet range test**
    - [ ] Deploy all 16 scarves in a festival-like space
    - [ ] Walk scarves apart to find range limit
    - [ ] Confirm all nodes receive broadcasts from all others
    - [ ] Evaluate whether flooding (TTL re-broadcast) is needed for range

## Investigations

- [ ] Investigate running on Zigbee (would require an upgraded ESP32)
- [ ] What refactorings would make this more extensible and modular?
- [ ] Figure out if there's a better way to form the network ad-hoc via button presses
