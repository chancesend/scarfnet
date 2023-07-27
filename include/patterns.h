#pragma once 

#include "defines.h"

#include <stdint.h>
#include <string>

class Pattern
{
public:
    virtual std::string name() = 0;
    virtual void paint(led_list& leds) = 0;
};

void fillNoise(led_list& leds, int32_t time);
void pride(led_list& leds);
void confetti(led_list& leds);
void firework(led_list& leds);
