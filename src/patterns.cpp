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
        pride(leds, ctx.timeMs, ctx.palette, ctx.beat);
    }});

    patterns.push_back({"confetti", [](Leds& leds, const PatternContext& ctx) {
        uint8_t fadeSpeed = rndRange(ctx.rnd,  1,  25);  // lower = slower fade
        uint8_t popChance = rndRange(ctx.rnd,  0, 100);  // % chance a pixel pops per tick
        // On beat: saturate pop chance for a dense burst, then let it decay normally.
        if (ctx.beat.isOnBeat(80)) popChance = 100;
        confetti(leds, ctx.timeMs, ctx.palette, fadeSpeed, popChance);
    }});

    patterns.push_back({"firework", [](Leds& leds, const PatternContext& ctx) {
        int periodMs = rndRange(ctx.rnd, 10, 85) * 100;  // launch cycle length
        // When a tempo is active, lock launch cadence to the beat.
        if (ctx.beat.isActive()) periodMs = ctx.beat.intervalMs;
        firework(leds, ctx.timeMs, periodMs, ctx.palette);
    }});

    patterns.push_back({"colorwaves", [](Leds& leds, const PatternContext& ctx) {
        colorwaves(leds, ctx.timeMs, ctx.palette);
        // Palette-colored swell on the beat — hue from rnd so each fleet session
        // gets a distinct accent color rather than always washing to white.
        if (ctx.beat.isActive()) {
            uint8_t flash = ctx.beat.flashBrightness(80);
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
        int    width         = rndRange(ctx.rnd,  1,  10);       // bar width in LEDs
        int    periodMs      = rndRange(ctx.rnd, 10,  60) * 100; // sweep cycle length
        fract8 blurAmount    = rndRange(ctx.rnd,  0, 100);       // edge softness
        uint8_t paletteShift = rndRange(ctx.rnd,  5,  15);       // palette scroll speed (bit-shift of timeMs)
        CRGB   color         = ColorFromPalette(ctx.palette, (ctx.timeMs >> paletteShift) % 255);
        // When a tempo is active, lock the sweep period to the beat so the bar
        // bounces end-to-end in sync with the music.
        if (ctx.beat.isActive()) periodMs = ctx.beat.intervalMs * 2;
        cylon(leds, ctx.timeMs, color, width, periodMs, blurAmount);
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
        dance(leds, ctx.timeMs, ctx.palette, ctx.beat, hueSpeed, macroPeriod);
    }});

    patterns.push_back({"generative", [](Leds& leds, const PatternContext& ctx) {
        // ctx.rnd offsets the macro vibe signals → different 20-min trajectory each press.
        // ctx.localRnd provides per-device spatial/phase offsets.
        generative(leds, ctx.timeMs, ctx.palette, ctx.beat, ctx.rnd, ctx.localRnd);
    }});

    std::string line = "Pattern list (" + std::to_string(patterns.size()) + " patterns): ";
    for (auto& pattern : patterns)
        line += pattern.first + ", ";
    Scarfnet::log(line.c_str());

    return patterns;
}

} // namespace Scarfnet
