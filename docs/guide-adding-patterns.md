# Guide: Adding a New LED Pattern

This covers everything needed to add a pattern from scratch. Four touch points total.

---

## What a pattern is

A pattern is a function called periodically (timed via `kLedRefreshRateMs`) that writes LED colors into the LED array. Pattern render functions live in `src/patterns/<name>.cpp`; their declarations are in `include/patterns.h`; the registry (`getPatternList`) lives in `src/patterns.cpp`.

The lambda signature (registered in `getPatternList`):

```cpp
[](Leds& leds, const PatternContext& ctx) { ... }
```

`PatternContext` bundles all inputs a pattern needs:

| Field        | Type              | What it is |
|-------------|-------------------|------------|
| `ctx.timeMs`    | `TimeMs`         | Synchronized mesh time in ms. **Always use this for animation** — not `millis()` — so all scarves stay phase-locked. |
| `ctx.palette`   | `CRGBPalette16`   | Current blended palette. |
| `ctx.rnd`       | `Rnd` (`uint16_t`) | Fleet-synced seed from the last button press. **Same on all scarves.** Use with `rndRange` to pick shared variation parameters. |
| `ctx.localRnd`  | `Rnd` (`uint16_t`) | Per-device seed, re-randomized independently on each pattern/seed change. **Unique per scarf, never transmitted.** Use for parameters that should differ between scarves independently. |
| `ctx.beat`      | `BeatInfo`         | Tap-tempo beat state. Use `ctx.beat.isActive()`, `ctx.beat.isOnBeat(windowMs)`, `ctx.beat.flashBrightness(windowMs)`. |

The render function itself is called from the lambda and may take any subset of these as typed parameters — see Step 1.

---

## Step 1 — Write the render function in `src/patterns/<name>.cpp`

```cpp
#include "patterns.h"

namespace Scarfnet {

void myPattern(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, int speed)
{
    // Use timeMs for all timing — never millis().
    for (int i = 0; i < kNumLeds; ++i) {
        uint8_t hue = (timeMs / speed + i * 8) % 256;
        leds[i] = ColorFromPalette(palette, hue, 200, LINEARBLEND);
    }
}

} // namespace Scarfnet
```

### Key helpers already in scope

- **`rndRange(rnd, low, high)`** — maps a `Rnd` seed deterministically into `[low, high)`. Use in the `getPatternList` lambda to convert the seed into typed parameter values:
  ```cpp
  int speed = rndRange(rnd, 10, 60);
  ```
- **`rndRange(val, low, high)`** — maps an arbitrary `uint8_t` index into `[low, high)`. Use inside render functions to map a computed 8-bit value into a parameter range.
- **`timeFrac8(timeMs, periodMs)`** — returns the fractional position (0–255) within a period. Use for looping animations:
  ```cpp
  uint8_t frac = timeFrac8(timeMs, 2000); // 0–255 over 2 seconds
  ```
- **`beatsin88(bpm, low, high)`** — sine wave oscillating between `low` and `high` at the given BPM. Uses an internal counter, **not** `timeMs` — use this only for slow global effects, not animation sync.
- **`fadeToBlackBy(leds.data(), kNumLeds, fade)`** — fade all LEDs toward black. Small values = slow fade. Use for trail/decay effects.
- **`ColorFromPalette(palette, index, brightness, blendType)`** — standard FastLED palette lookup.
- **`quadwave8(x)`** — smooth quadratic wave 0→255→0 over a 0–255 input. Good for bounce/cylon effects.

### Static variables — use with care

Some patterns (`pride`, `colorwaves`) use `static` variables to track inter-frame state. These work fine at runtime but **break unit tests** and can cause visual glitches when patterns switch because the statics persist. Prefer computing state from `timeMs` directly (stateless design) when possible.

```cpp
// Stateless (preferred): position derived from timeMs
int pos = (timeMs / 10) % kNumLeds;

// Stateful (only use if truly necessary):
static uint16_t accumulator = 0;
accumulator += deltaMs;  // requires tracking lastMs as another static
```

---

## Step 2 — Declare the render function in `include/patterns.h`

```cpp
void myPattern(Leds& leds, int32_t timeMs, const CRGBPalette16& palette, int speed);
```

Only the render function is declared here — not the lambda. The lambda lives entirely in `src/patterns.cpp`.

---

## Step 3 — Register in `getPatternList()` inside `src/patterns.cpp`

The lambda's job is to convert `ctx.rnd` / `ctx.localRnd` into named, typed parameters using `rndRange`, then call the render function.

```cpp
patterns.push_back({"myPattern", [](Leds& leds, const PatternContext& ctx) {
    int speed = rndRange(ctx.rnd, 10, 60);  // sweep speed in ms per step — shared across fleet
    myPattern(leds, ctx.timeMs, ctx.palette, speed);
}});
```

Use `ctx.rnd` for parameters you want to vary consistently across all scarves (everyone gets the same rnd, so all scarves pick the same speed). Use `ctx.localRnd` for parameters that should differ independently per device (e.g. grain size, phase offset).

The string name (e.g. `"myPattern"`) is what broadcasts over the mesh and what `changePatternFromString()` matches against. **Maximum 32 characters** (enforced by `HeartbeatPacket::pattern[33]`). Use only printable ASCII.

---

## Step 4 — Verify

1. Build: `pio run -e m5stack-atom-lite`
2. Flash and press the button until the new pattern appears.
3. Confirm animation runs smoothly solo.
4. With a second scarf, press the button and confirm the pattern name propagates within a heartbeat interval (5s).
5. Verify `rnd` produces noticeable variations of the main pattern and palette.

---

## Sync checklist

- [ ] All timing derived from `timeMs` — no `millis()`, no `EVERY_N_MILLISECONDS` inside the render function
- [ ] `rndRange` drives at least one visible parameter difference between scarves
- [ ] Pattern name ≤ 32 chars
- [ ] No `delay()` or blocking calls inside the render function
- [ ] If using `static` state: confirm it resets gracefully when the pattern is re-entered after a switch

---

## Ideas for new patterns

These would push the count from 6 toward the 8+ target:

| Name          | Status | Concept |
|---------------|--------|---------|
| `fractal`     | ✓ done | Three-octave fBm noise; slow 20-min macro drift + fast shimmer. `spatialScale` from `localRnd` varies grain per device |
| `breathe`     | ✓ done | Whole strip sine-wave brightness pulse, palette color drifts. Beat: white flash overlay |
| `sparkle`     | ✓ done | Fast-fade base with white spark pops; `sparkleRate` from `rnd`. Beat: dense burst |
| `dance`       | ✓ done | Beat-primary: sharp pulse on each beat, white burst + sparks on phrase boundary (every 8 beats) |


For "swarming" effects (LED patterns that highlight mesh sync visually), see the "Future Work" section in TODO.md — these patterns would intentionally phase-shift by `arrivalDelta` between nodes.
