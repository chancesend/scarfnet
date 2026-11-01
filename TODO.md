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
- [ ] Consistency: pattern functions take `int32_t timeMs` (negative after 24.8 days), add `__attribute__((format(printf,1,2)))` to `log()`
- [ ] Join bursts: every peer sends 3 heartbeats per join; consider having only one or two random peers respond

# Actual testing up at Burning Man
## Bugs found
- [ ] Can get into a state where 1 scarf isn't updating any of the others on button push, but is changing its own pattern (so we know it isn't crashed). This is fixed on reset of that scarf

