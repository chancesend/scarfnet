# TODO

## Pattern enhancements
- [ ] Add a couple of modes that attempt to do "swarming" logic by taking into account time-of-arrival delays between scarves, so that the LED patterns are not all the same (they are either offset in time, or do slightly different patterns for interesting visual effects)
    - [ ] Add very slight delay of secondary nodes when playing received pattern?
    - [ ] Can we make all the scarves extensions of the same pattern?
- [ ] Cylon pattern is messed up - maybe integer rollover problems?
- [ ] Overall LED brightness effected by flashes due to current limiting - can this be fixed? Might need a capacitor 
- [ ] Investigate a wild phrase ending mode where each scarf picks a sub-beat in the last couple of beats and randomly goes wild on that sub-beat. The overall effect is that the wildness is spreading across all the scarves at slightly different times

## Investigations

- [ ] Investigate running on Zigbee (would require an upgraded ESP32)
- [ ] What refactorings would make this more extensible and modular?
- [ ] Figure out if there's a better way to form the network ad-hoc via button presses
