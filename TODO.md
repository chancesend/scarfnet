# TODO

- [ ] Refactor the patterns so that the interfaces are more modular
    - [ ] Change the debug pattern context info so that it's not so specialized

## Pattern enhancements
- [ ] Add a couple of modes that attempt to do "swarming" logic by taking into account time-of-arrival delays between scarves, so that the LED patterns are not all the same (they are either offset in time, or do slightly different patterns for interesting visual effects)
    - [ ] Add very slight delay of secondary nodes when playing received pattern?
    - [ ] Can we make all the scarves extensions of the same pattern?

## Physical upgrades
- [ ] Overall LED brightness effected by flashes due to current limiting - can this be fixed with a capacitor?
- [ ] Let's just solder and heat-shrink the connector to the LED strip 

## Investigations

- [ ] Investigate running on Zigbee/Thread (would require an upgraded ESP32)
- [ ] What refactorings would make this more extensible and modular?
- [ ] Figure out if there's a better way to form the network ad-hoc via button presses
- [x] Figure out why some color fading seems to be less smooth / choppier than desired

# Actual testing up at Burning Man
## Bugs found
- [ ] If a scarf misses the update from a button push of another scarf, then it won't ever catch up when receiving future heartbeats (this is the main bug preventing the network from being resiliant to network issues)
- [ ] Only 1 scarf should be able to get into tempo mode at once, and any other scarves pressed should put the whole network out of tempo mode (including the one that initially entered tempo mode). This will solve problems that occur if 2 scarves get into tempo mode and try to compete - with a fix, the 2nd scarf that gets into tempo mode will get the other scarf out of tempo mode
- [ ] Possible problem when a new scarf is added after others have been on for a long time
- [ ] Can get into a state where 1 scarf isn't updating any of the others on button push, but is changing its own pattern (so we know it isn't crashed). This is fixed on reset of that scarf

## More things to explore
- [ ] Is there any way to test network errors and missed packets via simulation?
- [ ] What other interesting patterns/modes can we implement using completely unique methods of light visualizations? ie, turns out the debug pattern is super fun and interesting when you have a ton of scarves, and it operates via a completely different method than the other patterns