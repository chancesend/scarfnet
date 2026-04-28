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
        if (ctx.beat.isOnBeat(80)) popChance = 400;
        if (ctx.beat.isOnBar(120)) popChance = 800;
        confetti(leds, ctx.timeMs, ctx.palette, fadeSpeed, popChance);
    }});

    patterns.push_back({"firework", [](Leds& leds, const PatternContext& ctx) {
        firework(leds, ctx);
    }});

    patterns.push_back({"colorwaves", [](Leds& leds, const PatternContext& ctx) {
        colorwaves(leds, ctx);
    }});

    patterns.push_back({"fractal", [](Leds& leds, const PatternContext& ctx) {
        // Use localRnd so each scarf has a stable, unique grain that differs from its peers.
        uint8_t spatialScale = rndRange(ctx.localRnd, 4, 14);
        fractal(leds, ctx.timeMs, ctx.palette, ctx.beat, spatialScale, ctx.rnd);
    }});

    patterns.push_back({"cylon", [](Leds& leds, const PatternContext& ctx) {
        cylon(leds, ctx);
    }});

    patterns.push_back({"breathe", [](Leds& leds, const PatternContext& ctx) {
        int32_t periodMs = rndRange16(ctx.rnd, 20, 80) * 100;  // breath cycle: 2–8 s
        uint8_t hueSpeed = rndRange(ctx.rnd, 50, 200);         // larger = slower color drift
        breathe(leds, ctx.timeMs, ctx.palette, ctx.beat, periodMs, hueSpeed, ctx.rnd);
    }});

    patterns.push_back({"sparkle", [](Leds& leds, const PatternContext& ctx) {
        sparkle(leds, ctx);
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
