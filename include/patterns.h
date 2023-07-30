#pragma once 

#include "defines.h"

#include <stdint.h>
#include <string>

namespace Scarf {

class Pattern
{
public:
    virtual std::string name() = 0;
    virtual void paint(Leds& leds, int32_t timeMs) = 0;
};

typedef std::function<void(Leds&, int32_t, int32_t)> PatternFcn;
typedef std::pair<std::string, PatternFcn> NamedPattern;
typedef std::vector<NamedPattern> PatternList;

void fillNoise(Leds& leds, int32_t timeMs);
void pride(Leds& leds, int32_t timeMs, CRGBPalette16 palette);
void confetti(Leds& leds, int32_t timeMs, CRGBPalette16 palette, uint8_t fade, uint8_t popChancePct);
void firework(Leds& leds, int32_t timeMs, int32_t periodMs, CRGBPalette16 palette);
void cylon(Leds& leds, int32_t timeMs, CRGB c, int width, int periodMs, fract8 blurAmount);

void getPatternList(PatternList& patterns);

}
