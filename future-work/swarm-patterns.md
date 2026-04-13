# Swarm / Constellation Patterns

## Goal

Each scarf shows a slightly different but correlated LED pattern so that the
group as a whole produces a "sweep" or ripple effect rather than everyone
flashing identically.

---

## Why physical ToA positioning won't work

The naive approach — use packet time-of-arrival differences to triangulate
physical positions — is not feasible with WiFi mesh:

- Physical propagation at 1–10 m adds **~3–30 ns** of delay.
- painlessMesh protocol overhead is **1–30 ms**.
- The physical signal is completely buried in processing noise.

---

## What can work: network-topology-based proximity

Even though we can't get physical positions, network latency is still
**spatially correlated** in a dense peer group:

- A scarf 1 m away is almost certainly a single hop with low RTT.
- A scarf 15 m away at the edge of a room may be 2–3 hops with higher RTT.

This gives a meaningful "near vs. far" gradient, not geometry.

### The key signal: one-way arrival delta

Every heartbeat already carries `currentTimeMs` (sender's mesh clock at send
time). Since painlessMesh synchronizes clocks across the network:

```
arrivalDelta = receiverTimeMs - senderTimeMs  ≈  one-way propagation delay
```

Per-node deltas can be smoothed with an exponential moving average (EMA) over
successive heartbeats to produce a stable "network distance" estimate for each
peer.

---

## Proposed visual mechanic: ripple from source

When a scarf presses the button:

1. That scarf shows the new pattern at **phase 0** (no offset).
2. Other scarves offset their animation time by:
   ```
   animationOffset = arrivalDelta_to_sender × kSwarmAmplification
   ```
   where `kSwarmAmplification` (e.g. 50–200×) makes the small network delays
   visible as perceptible animation phase differences.
3. The result: the animation appears to "spread" outward from the source
   scarf through the group, like a ripple.

---

## Implementation plan

### Phase 1 — Measurement & validation (done / in progress)

- [x] Add per-node `arrivalDelta` tracking to `Mesh` (EMA-smoothed)
- [x] Log raw and smoothed deltas on every received heartbeat
- [ ] Run with 2–4 scarves and observe: Are the deltas stable? Consistent
      across sessions? Do they correlate with physical proximity?

### Phase 2 — Animation offset

- [ ] `Scarf` stores `_lastPatternSenderNodeId` when accepting a pattern change
- [ ] `Scarf` computes `_animationOffsetMs = mesh.getArrivalDelta(sender) × kSwarmAmplification`
- [ ] `PatternManager::runCurrentPattern` receives `nodeTimeMs + _animationOffsetMs`
      instead of raw `nodeTimeMs`
- [ ] Add `kSwarmAmplification` to `config.h`; tune empirically

### Phase 3 — Dedicated sweep pattern

A pattern with a strong directional wave character will make the phase offsets
most visible. Ideas:

- **Color sweep**: a hue gradient that slides along the strip; offset phases
  make adjacent scarves appear to "pass" the color wave to each other.
- **Brightness ripple**: all scarves show the same color but brightness peaks
  travel through the group.
- **Complementary colors**: nearby scarves (small delta) show warm hues,
  distant ones show cool hues.

---

## Open questions

1. **Delta stability**: are the EMA-smoothed values stable enough over a
   full evening's wear, or do they drift as people move around?
2. **Negative deltas**: clock sync isn't perfect; deltas can be negative
   (slightly). Should negative deltas be clamped to 0 or treated as "closest
   possible neighbor"?
3. **Single-scarf fallback**: if only one scarf is present (no peers), offset
   is 0 — normal behavior. No special case needed.
4. **Amplification tuning**: 50× on 10 ms raw delay = 500 ms offset. With a
   3-second pattern cycle that's a ~17% phase shift — probably visible. Needs
   empirical testing.
