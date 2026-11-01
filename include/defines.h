#pragma once

#if SCARFNET_EMBEDDED
#include <FastLED.h>
#else
#include "fastled_stub.h"
#endif

#include <string>
#include <vector>

const int kNumLeds = 25;

#if SCARFNET_EMBEDDED
const int kLedPin  = 26;
const int kBuiltinLedPin = 27;
const int kNumBuiltinLeds = 1;
const int kButtonPin = 39;

enum ELedType: int {
    kLedType_Adafruit = 0,  // GRB — Adafruit NeoPixel Dots
    kLedType_Amazon = 1,    // RGB — ALITOVE WS2811 and some other generic Amazon strands
    kLedType_Bike = 2,      // BGR — Adafruit NeoPixel LED Balls used in bike install

    kLedType_Count,
};

template<uint8_t DATA_PIN> class ADAFRUIT : public WS2812Controller800Khz<DATA_PIN, GRB> {};
template<uint8_t DATA_PIN> class AMAZON : public WS2812Controller800Khz<DATA_PIN, RGB> {};
template<uint8_t DATA_PIN> class BIKE : public WS2812Controller800Khz<DATA_PIN, BGR> {};
template<uint8_t DATA_PIN> class M5_INTERNAL_TYPE : public WS2812Controller800Khz<DATA_PIN, GRB> {};

#endif // SCARFNET_EMBEDDED

#include "typedefs.h"
