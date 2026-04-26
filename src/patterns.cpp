#include "patterns.h"
#include "log.h"

#include <cstdio>
#include <string>

namespace Scarfnet {

PatternList getPatternList()
{
    Scarfnet::log("getPatternList()");
    PatternList patterns;

    patterns.push_back({"pride", [](Leds& leds, const PatternContext& ctx) {
        pride(leds, ctx.timeMs, ctx.palette, ctx.beat, ctx.rnd, ctx.localRnd);
    }});

    patterns.push_back({"confetti", [](Leds& leds, const PatternContext& ctx) {
        uint8_t fadeSpeed = rndRange(ctx.rnd,  1,  25);  // lower = slower fade
        uint16_t popChance = rndRange(ctx.rnd,  0, 100);  // % chance a pixel pops per tick
        // On beat: saturate pop chance for a dense burst, then let it decay normally.
        if (ctx.beat.isOnBeat(80)) popChance = 500;
        confetti(leds, ctx.timeMs, ctx.palette, fadeSpeed, popChance);
    }});

    patterns.push_back({"firework", [](Leds& leds, const PatternContext& ctx) {
        int periodMs = rndRange(ctx.rnd, 10, 85) * 100;  // launch cycle length
        // When a tempo is active, lock launch cadence to the beat.
        if (ctx.beat.isActive()) periodMs = ctx.beat.intervalMs;
        firework(leds, ctx.timeMs, periodMs, ctx.palette);
    }});

    patterns.push_back({"colorwaves", [](Leds& leds, const PatternContext& ctx) {
        colorwaves(leds, ctx.timeMs, ctx.palette, ctx.beat);
        // Palette-colored swell on the beat — hue from rnd so each fleet session
        // gets a distinct accent color rather than always washing to white.
        if (ctx.beat.isActive()) {
            uint16_t flashWindow = rndRange(ctx.localRnd xor ctx.timeMs, 0, 255);  // randomised decay window
            uint8_t flash = ctx.beat.sawTime(flashWindow);
            if (flash > 0) {
                CRGB flashColor = ColorFromPalette(ctx.palette, (uint8_t)(ctx.rnd * 17u >> 8), 255, LINEARBLEND);
                for (auto& led : leds) led = blend(led, flashColor, flash >> 2);
            }
        }
    }});

    patterns.push_back({"fractal", [](Leds& leds, const PatternContext& ctx) {
        // Use localRnd so each scarf has a stable, unique grain that differs from its peers.
        uint8_t spatialScale = rndRange(ctx.localRnd, 4, 14);
        fractal(leds, ctx.timeMs, ctx.palette, ctx.beat, spatialScale, ctx.rnd);
    }});

    patterns.push_back({"cylon", [](Leds& leds, const PatternContext& ctx) {
        int width = rndRange(ctx.rnd, 6, 16);  // bar is ~7–15 pixels wide
        // Non-beat: very slow 30–80 s sweep; beat-locked: 2× beat interval
        int basePeriodMs = ctx.beat.isActive()
                           ? (int)ctx.beat.intervalMs * 2
                           : rndRange(ctx.rnd, 30, 80) * 1000;
        // Base hue drifts very slowly through the palette (~4 min full cycle)
        uint8_t baseHue      = (uint8_t)(ctx.timeMs >> 10);
        // Secondary: session-fixed palette offset for the center accent
        uint8_t secondaryHue = baseHue + rndRange(ctx.rnd >> 4, 64, 192);
        cylon(leds, ctx.timeMs, ctx.palette, baseHue, secondaryHue, width, basePeriodMs);
    }});

    patterns.push_back({"breathe", [](Leds& leds, const PatternContext& ctx) {
        int32_t periodMs = rndRange16(ctx.rnd, 20, 80) * 100;  // breath cycle: 2–8 s
        uint8_t hueSpeed = rndRange(ctx.rnd, 50, 200);         // larger = slower color drift
        breathe(leds, ctx.timeMs, ctx.palette, ctx.beat, periodMs, hueSpeed, ctx.rnd);
    }});

    patterns.push_back({"sparkle", [](Leds& leds, const PatternContext& ctx) {
        uint8_t sparkleRate = rndRange(ctx.rnd, 1, 6);  // very sparse baseline

        // Organic burst every ~2 min: slow noise (ctx.rnd shifts burst timing per fleet
        // press so the burst cadence isn't always at the same point in the session).
        // inoise8 clusters around 128; values above 195 are uncommon — roughly 1-2
        // sustained bursts per 20-minute session, each lasting 20-60 seconds.
        uint8_t burstNoise = inoise8((uint32_t)ctx.timeMs / 18000, (uint8_t)ctx.rnd);
        if (burstNoise > 195) {
            uint8_t excess = burstNoise - 195;      // 0–60 typical range
            sparkleRate = qadd8(sparkleRate, min((int)excess * 3, 70));
        }

        // On beat: moderate pop, not a full whiteout
        if (ctx.beat.isOnBeat(80)) sparkleRate = qadd8(sparkleRate, 60);

        sparkle(leds, ctx.timeMs, ctx.palette, sparkleRate);
    }});

    patterns.push_back({"dance", [](Leds& leds, const PatternContext& ctx) {
        uint8_t hueSpeed    = rndRange(ctx.rnd, 80, 220);   // color drift rate
        uint8_t macroPeriod = (ctx.rnd & 1) ? 64 : 32;     // wild window every 32 or 64 beats
        dance(leds, ctx.timeMs, ctx.palette, ctx.beat, hueSpeed, macroPeriod, ctx.localRnd);
    }});

    patterns.push_back({"generative", [](Leds& leds, const PatternContext& ctx) {
        // ctx.rnd offsets the macro vibe signals → different 20-min trajectory each press.
        // ctx.localRnd provides per-device spatial/phase offsets.
        generative(leds, ctx.timeMs, ctx.palette, ctx.beat, ctx.rnd, ctx.localRnd);
    }});

    patterns.push_back({"beat", [](Leds& leds, const PatternContext& ctx) {
        beat(leds, ctx.timeMs, ctx.palette, ctx.beat, ctx.rnd, ctx.localRnd);
    }});

    patterns.push_back({"digital", [](Leds& leds, const PatternContext& ctx) {
        digital(leds, ctx.timeMs, ctx.palette, ctx);
    }});

    patterns.push_back({"debug", [](Leds& leds, const PatternContext& ctx) {
        debug(leds, ctx);
    }});

    std::string line = "Pattern list (" + std::to_string(patterns.size()) + " patterns): ";
    for (auto& pattern : patterns)
        line += pattern.first + ", ";
    Scarfnet::log(line.c_str());

    return patterns;
}

} // namespace Scarfnet
