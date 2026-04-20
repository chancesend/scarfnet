#include "patterns.h"
#include "log.h"

#include <Arduino.h>

namespace Scarfnet {

PatternList getPatternList()
{
    Scarfnet::log("getPatternList()");
    PatternList patterns;

    patterns.push_back({"pride", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd randomizer) {
        pride(leds, timeMs, palette);
    }});

    patterns.push_back({"confetti", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd randomizer) {
        uint8_t thisFade  = interpUint8(randomizer % 25, 1, 25);
        uint8_t popChance = interpUint8((randomizer % 25) * 4, 0, 100);
        confetti(leds, timeMs, palette, thisFade, popChance);
    }});

    patterns.push_back({"firework", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd randomizer) {
        const uint8_t periodInterp = interpUint8((randomizer % 25) * 3, 10, 85);
        firework(leds, timeMs, periodInterp * 100, palette);
    }});

    patterns.push_back({"colorwaves", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd randomizer) {
        colorwaves(leds, timeMs, palette);
    }});

    patterns.push_back({"cylon", [](Leds& leds, int32_t timeMs, CRGBPalette16 palette, Rnd randomizer) {
        int      width               = interpUint8((randomizer % 25) * 10, 1, 10);
        uint8_t  periodInterp        = interpUint8((randomizer % 25) * 2,  10, 60);
        fract8   blurAmount          = interpUint8((randomizer % 25) * 4,  0, 100);
        auto     paletteChangeDivisor = randomizer % 10 + 5;
        CRGB     color               = ColorFromPalette(palette, (timeMs >> paletteChangeDivisor) % 255);
        cylon(leds, timeMs, color, width, periodInterp * 100, blurAmount);
    }});

    String line = String("Pattern list (") + patterns.size() + " patterns): ";
    for (auto& pattern : patterns)
        line += pattern.first.c_str() + String(", ");
    Scarfnet::log(line.c_str());

    return patterns;
}

} // namespace Scarfnet
