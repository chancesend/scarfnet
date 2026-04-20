#include "patterns.h"
#include "log.h"

#include <Arduino.h>

namespace Scarfnet {

PatternList getPatternList()
{
    Scarfnet::log("getPatternList()");
    PatternList patterns;

    patterns.push_back({"pride", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd rnd, const BeatInfo& beat) {
        pride(leds, timeMs, palette, beat);
    }});

    patterns.push_back({"confetti", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd rnd, const BeatInfo& beat) {
        uint8_t fadeSpeed = rndRange(rnd,  1,  25);  // lower = slower fade
        uint8_t popChance = rndRange(rnd,  0, 100);  // % chance a pixel pops per tick
        confetti(leds, timeMs, palette, fadeSpeed, popChance);
    }});

    patterns.push_back({"firework", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd rnd, const BeatInfo& beat) {
        int periodMs = rndRange(rnd, 10, 85) * 100;  // launch cycle length
        firework(leds, timeMs, periodMs, palette);
    }});

    patterns.push_back({"colorwaves", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd rnd, const BeatInfo& beat) {
        colorwaves(leds, timeMs, palette);
    }});

    patterns.push_back({"cylon", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd rnd, const BeatInfo& beat) {
        int    width         = rndRange(rnd,  1,  10);       // bar width in LEDs
        int    periodMs      = rndRange(rnd, 10,  60) * 100; // sweep cycle length
        fract8 blurAmount    = rndRange(rnd,  0, 100);       // edge softness
        uint8_t paletteShift = rndRange(rnd,  5,  15);       // palette scroll speed (bit-shift of timeMs)
        CRGB   color         = ColorFromPalette(palette, (timeMs >> paletteShift) % 255);
        cylon(leds, timeMs, color, width, periodMs, blurAmount);
    }});

    String line = String("Pattern list (") + patterns.size() + " patterns): ";
    for (auto& pattern : patterns)
        line += pattern.first.c_str() + String(", ");
    Scarfnet::log(line.c_str());

    return patterns;
}

} // namespace Scarfnet
