#include "patterns.h"
#include "log.h"

#include <Arduino.h>

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
        // Subtle brightness swell on the beat — gentler than pride's full flash.
        if (ctx.beat.isActive()) {
            uint8_t flash = ctx.beat.flashBrightness(80);
            if (flash > 0)
                for (auto& led : leds) led = blend(led, CRGB::White, flash >> 2);
        }
    }});

    patterns.push_back({"fractal", [](Leds& leds, const PatternContext& ctx) {
        // Use localRnd so each scarf has a stable, unique grain that differs from its peers.
        uint8_t spatialScale = rndRange(ctx.localRnd, 4, 14);
        fractal(leds, ctx.timeMs, ctx.palette, ctx.beat, spatialScale);
    }});

    patterns.push_back({"cylon", [](Leds& leds, const PatternContext& ctx) {
        int    width         = rndRange(ctx.rnd,  1,  10);       // bar width in LEDs
        int    periodMs      = rndRange(ctx.rnd, 10,  60) * 100; // sweep cycle length
        fract8 blurAmount    = rndRange(ctx.rnd,  0, 100);       // edge softness
        uint8_t paletteShift = rndRange(ctx.rnd,  5,  15);       // palette scroll speed (bit-shift of timeMs)
        CRGB   color         = ColorFromPalette(ctx.palette, (ctx.timeMs >> paletteShift) % 255);
        // When a tempo is active, lock the sweep period to the beat so the bar
        // bounces end-to-end in sync with the music.
        if (ctx.beat.isActive()) periodMs = ctx.beat.intervalMs;
        cylon(leds, ctx.timeMs, color, width, periodMs, blurAmount);
    }});

    patterns.push_back({"breathe", [](Leds& leds, const PatternContext& ctx) {
        int32_t periodMs = rndRange16(ctx.rnd, 20, 80) * 100;  // breath cycle: 2–8 s
        uint8_t hueSpeed = rndRange(ctx.rnd, 50, 200);         // larger = slower color drift
        breathe(leds, ctx.timeMs, ctx.palette, ctx.beat, periodMs, hueSpeed);
    }});

    patterns.push_back({"sparkle", [](Leds& leds, const PatternContext& ctx) {
        uint8_t sparkleRate = rndRange(ctx.rnd, 2, 30);       // baseline spark density
        // On beat: saturate the strip with a white burst
        if (ctx.beat.isOnBeat(80)) sparkleRate = 255;
        sparkle(leds, ctx.timeMs, ctx.palette, sparkleRate);
    }});

    patterns.push_back({"dance", [](Leds& leds, const PatternContext& ctx) {
        uint8_t hueSpeed = rndRange(ctx.rnd, 80, 220);        // color drift rate
        dance(leds, ctx.timeMs, ctx.palette, ctx.beat, hueSpeed);
    }});

    String line = String("Pattern list (") + patterns.size() + " patterns): ";
    for (auto& pattern : patterns)
        line += pattern.first.c_str() + String(", ");
    Scarfnet::log(line.c_str());

    return patterns;
}

} // namespace Scarfnet
