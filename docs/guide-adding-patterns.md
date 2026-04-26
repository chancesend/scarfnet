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

| Field | Type | What it is |
|---|---|---|
| `ctx.timeMs` | `TimeMs` (`int32_t`) | Synchronized mesh time in ms. **Always use this for animation** — not `millis()` — so all scarves stay phase-locked. |
| `ctx.palette` | `CRGBPalette16` | Current blended palette. |
| `ctx.rnd` | `Rnd` (`uint16_t`) | Fleet-synced seed from the last button press. **Same on all scarves.** Use with `rndRange` to pick variation parameters that are shared across the fleet. |
| `ctx.localRnd` | `Rnd` (`uint16_t`) | Per-device seed, re-randomized independently on each pattern/seed change. **Unique per scarf, never transmitted.** Use for parameters that should differ between scarves (grain, phase offset, LED center). |
| `ctx.beat` | `BeatInfo` | Tap-tempo beat state. See the full API below. |
| `ctx.nodeId` | `NodeId` (`uint32_t`) | This scarf's own node ID. Stable per device. |
| `ctx.lastPressId` | `NodeId` | Node ID that last changed the pattern. Matches `ctx.nodeId` on the scarf that pressed. |
| `ctx.recentFlashes` | `NodeFlash[16]` | Ring buffer of recent heartbeat arrivals (id + arrival time). Used by the debug pattern for per-node flash effects. |
| `ctx.flashCount` | `int` | Number of valid entries in `ctx.recentFlashes`. |

The render function itself is called from the lambda and may take any subset of these as typed parameters — see Step 1.

---

## BeatInfo API

`ctx.beat` is a `BeatInfo` snapshot. All methods return a safe default (`start` value or `false`) when `!isActive()`, so patterns don't need to guard every call.

### Activation & raw fields

```cpp
ctx.beat.isActive()           // true once tap-tempo is locked
ctx.beat.intervalMs           // beat period in ms (0 = inactive)
ctx.beat.phaseMs              // ms elapsed since last beat onset (0 = on the beat)
ctx.beat.beatNumber           // total beats since tempo was established
ctx.beat.tempo()              // BPM as float; 0 when inactive
ctx.beat.frac8()              // fractional position within beat [0..255], 0 = onset
```

### Beat-level helpers

```cpp
ctx.beat.isOnBeat(windowMs)   // true within windowMs of any beat onset (default 60ms)
```

### Envelope methods

All envelope methods return a `uint8_t` (0–255) and safely return `start` when inactive.

```cpp
// Percussive decay: start→end over windowMs after onset, then holds end.
// Most common use: bright flash on the hit that fades out quickly.
ctx.beat.sawTime(windowMs = 60, start = 255, end = 0)

// Saw over a fraction of the period. duty=1.0 = full period, 0.5 = first half.
// phase [0..1] shifts the ramp within the period.
ctx.beat.saw(start = 0, end = 255, duty = 1.0f, phase = 0.0f)

// Triangle: start→end→start over duty fraction of period.
ctx.beat.triangle(start = 0, end = 255, duty = 1.0f, phase = 0.0f)

// Smooth bell curve (half-sine): peaks at end at midbeat, returns to start.
ctx.beat.sin(start = 0, end = 255)

// Square: end for first duty/255 of period, start after.
// duty=128 → 50%, duty=64 → 25%.
ctx.beat.square(duty = 128, start = 0, end = 255)

// Exponential-ish decay (squared falloff): punchy, natural for percussive hits.
ctx.beat.expDecay(start = 0, end = 255)
```

### Bar-level

```cpp
ctx.beat.beatInBar(beatsPerBar = 4)           // beat within bar, 0-based (0 = downbeat)
ctx.beat.barNumber(beatsPerBar = 4)           // bars since tempo started, 0-based
ctx.beat.isOnBar(beatsPerBar = 4, windowMs = 60)  // true within windowMs of a bar downbeat
ctx.beat.barPhaseMs(beatsPerBar = 4)          // ms elapsed since the last bar downbeat
```

### Phrase-level

```cpp
ctx.beat.isOnPhrase(phraseBeats, windowMs = 60)   // true within windowMs of an N-beat boundary
ctx.beat.isLastBeatOfPhrase(phraseBeats)           // true on the final beat before the next boundary
ctx.beat.phraseFrac8(phraseBeats)                  // position within phrase [0..255]
ctx.beat.phraseFrac16(phraseBeats)                 // position within phrase [0..65535], higher precision
```

### phraseView — treat a phrase like a single beat

`phraseView(n)` returns a synthetic `BeatInfo` whose "beat period" spans `n` real beats. This lets you reuse any of the beat-level envelope methods (`saw`, `triangle`, `sin`, etc.) over a whole phrase without writing separate phrase math:

```cpp
// Rising saw brightness over 16 beats
uint8_t bright = ctx.beat.phraseView(16).saw(40, 255);

// Smooth swell peaking at the midpoint of a 32-beat phrase
uint8_t swell  = ctx.beat.phraseView(32).sin(60, 255);

// Percussive hit at the top of each 8-beat phrase
uint8_t hit    = ctx.beat.phraseView(8).sawTime(200);
```

### Compound position

```cpp
// Full bar/beat/subdivision breakdown.
BarBeat pos = ctx.beat.position(beatsPerBar = 4, subdivisionsPerBeat = 2);
// pos.bar          — bar number (0-based)
// pos.beat         — beat within bar (0-based)
// pos.subdivision  — subdivision within beat (0-based)
// pos.beatFrac8    — fractional position within beat [0..255]
```

---

## Step 1 — Write the render function in `src/patterns/<name>.cpp`

```cpp
#include "patterns.h"

namespace Scarfnet {

void myPattern(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
               const BeatInfo& beat, int speed)
{
    if (!beat.isActive()) {
        // Graceful fallback when no tap-tempo is set
        uint8_t hue = (uint8_t)(timeMs / speed);
        for (auto& led : leds)
            led = ColorFromPalette(palette, hue, 180, LINEARBLEND);
        return;
    }

    uint8_t bright = beat.sawTime(beat.intervalMs / 4);  // sharp percussive hit
    uint8_t hue    = (uint8_t)(timeMs / speed);
    for (int i = 0; i < kNumLeds; ++i)
        leds[i] = ColorFromPalette(palette, hue + i * 8, bright, LINEARBLEND);
}

} // namespace Scarfnet
```

### Key helpers already in scope

- **`rndRange(rnd, low, high)`** — maps a `Rnd` seed deterministically into `[low, high)`. Use in the `getPatternList` lambda to convert a seed into typed parameter values:
  ```cpp
  int speed = rndRange(ctx.rnd, 10, 60);
  ```
- **`timeFrac8(timeMs, periodMs)`** — fractional position (0–255) within a period:
  ```cpp
  uint8_t frac = timeFrac8(timeMs, 2000); // 0–255 over 2 seconds
  ```
- **`fadeToBlackBy(leds.data(), kNumLeds, fade)`** — fade all LEDs toward black. Small values = slow fade. Good for trail/decay effects.
- **`ColorFromPalette(palette, index, brightness, blendType)`** — standard FastLED palette lookup.
- **`quadwave8(x)`** — smooth quadratic wave 0→255→0 over a 0–255 input. Good for bounce/cylon effects.

### Static variables — use with care

Some patterns (`pride`, `colorwaves`) use `static` variables to track inter-frame state. These work fine at runtime but **break unit tests** and can cause visual glitches when patterns switch because statics persist. Prefer deriving state from `timeMs` directly (stateless design) when possible.

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
void myPattern(Leds& leds, int32_t timeMs, const CRGBPalette16& palette,
               const BeatInfo& beat, int speed);
```

Only the render function is declared here — not the lambda. The lambda lives entirely in `src/patterns.cpp`.

---

## Step 3 — Register in `getPatternList()` inside `src/patterns.cpp`

The lambda converts `ctx.rnd` / `ctx.localRnd` into named, typed parameters via `rndRange`, then calls the render function.

```cpp
patterns.push_back({"myPattern", [](Leds& leds, const PatternContext& ctx) {
    int speed = rndRange(ctx.rnd, 10, 60);  // shared across fleet — same on all scarves
    myPattern(leds, ctx.timeMs, ctx.palette, ctx.beat, speed);
}});
```

Use `ctx.rnd` for parameters that should match across the fleet (same seed → same variation). Use `ctx.localRnd` for parameters that should differ independently per device (grain, phase, spatial offset).

The string name (e.g. `"myPattern"`) is what broadcasts over the mesh and what `changePatternFromString()` matches against. **Maximum 31 characters** (pattern field is 32 bytes including null terminator, enforced by `static_assert`). Use only printable ASCII.

---

## Step 4 — Verify

1. Build: `pio run -e m5stack-atom-lite`
2. Run in simulator first: `cd tools/sim && make run` — use `SPACE` to cycle to the new pattern, `T` to tap tempo.
3. Flash and confirm animation runs smoothly solo.
4. With a second scarf, press the button and confirm the pattern name propagates within a heartbeat interval (5s).
5. Verify `rnd` produces noticeable variations of the main pattern and palette.

---

## Sync checklist

- [ ] All timing derived from `ctx.timeMs` — no `millis()`, no `EVERY_N_MILLISECONDS` inside the render function
- [ ] Graceful fallback for `!beat.isActive()` — pattern should still look good without a tempo
- [ ] `rndRange(ctx.rnd, ...)` drives at least one visible parameter difference across the fleet
- [ ] Pattern name ≤ 31 chars
- [ ] No `delay()` or blocking calls inside the render function
- [ ] If using `static` state: confirm it resets gracefully when the pattern is re-entered after a switch

---

## Current pattern list

| Name | Concept |
|---|---|
| `debug` | Per-node color derived from node ID; flashes neighbor colors on heartbeat receipt |
| `pride` | Classic rainbow with beat-driven brightness swell |
| `confetti` | Random pixel pops with fade; beat triggers dense burst |
| `firework` | Periodic expanding burst; locks cadence to beat when active |
| `colorwaves` | Slow palette color wash; beat-driven flash accent |
| `fractal` | Three-octave fBm noise; `spatialScale` from `localRnd` varies grain per device |
| `cylon` | Bouncing bar; sweep period locks to 2× beat when active |
| `breathe` | Whole-strip sine-wave brightness pulse; beat: white flash overlay |
| `sparkle` | Sparse pixel pops; organic burst via slow noise; beat: dense pop |
| `dance` | Beat-primary radial flash with locally-random center; cylon underlay; phrase events |
| `generative` | Slow macro-vibe signals drive multiple simultaneous effects |
| `digital` | Falling-code columns; beat-synced |
