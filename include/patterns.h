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

void fillNoise(Leds& leds, int32_t timeMs);
void pride(Leds& leds, int32_t timeMs);
void confetti(Leds& leds, int32_t timeMs);
void firework(Leds& leds, int32_t timeMs, int32_t periodMs);
void cylon(Leds& leds, int32_t timeMs, CRGB c, int width, int periodMs);

}
