# TODO

- [ ] Refactor the patterns so that the interfaces are more modular
    - [ ] Change the debug pattern context info so that it's not so specialized

## Pattern enhancements
- [ ] Add a couple of modes that attempt to do "swarming" logic by taking into account time-of-arrival delays between scarves, so that the LED patterns are not all the same (they are either offset in time, or do slightly different patterns for interesting visual effects)
    - [ ] Add very slight delay of secondary nodes when playing received pattern?
    - [ ] Can we make all the scarves extensions of the same pattern?
- [ ] Overall LED brightness effected by flashes due to current limiting - can this be fixed? Might need a capacitor 

## Investigations

- [ ] Investigate running on Zigbee (would require an upgraded ESP32)
- [ ] What refactorings would make this more extensible and modular?
- [ ] Figure out if there's a better way to form the network ad-hoc via button presses
- [x] Figure out why some color fading seems to be less smooth / choppier than desired
- [ ] Program it so that when one scarf is in tap-tempo mode, when another scarf enters tap-tempo mode it will sync to the currently tapped tempo, and that scarf can then adjust the tempo through normal tap methods