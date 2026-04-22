#pragma once

#include <stdint.h>
#include <vector>
#if SCARFNET_EMBEDDED
#include <FastLED.h>    // For CRGB
#endif

typedef std::vector<CRGB> Leds;
typedef uint16_t Rnd;
