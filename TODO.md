# TODO

## Pattern enhancements
- [ ] Add a couple of modes that attempt to do "swarming" logic by taking into account time-of-arrival delays between scarves, so that the LED patterns are not all the same (they are either offset in time, or do slightly different patterns for interesting visual effects)
    - [ ] Add very slight delay of secondary nodes when playing received pattern?
    - [ ] Can we make all the scarves extensions of the same pattern?
- [ ] Work on "digital" effect - 1-4    
    LEDs are added and removed on the beat. Then at the end of every     
    16/32/64 beat pattern, the colors and speed goes a little wild       
    (with a bit of randomness thrown in)
- [ ] Overall LED brightness effected by flashes due to current limiting - can this be fixed? Might need a capacitor
- [ ] For the "pride" mode, it should be that every 16/32/64 beat there's a random chance that the pattern will speed up for a beat or two, or that the colors will shift really quickly  

## Investigations

- [ ] Investigate running on Zigbee (would require an upgraded ESP32)
- [ ] What refactorings would make this more extensible and modular?
- [ ] Figure out if there's a better way to form the network ad-hoc via button presses
