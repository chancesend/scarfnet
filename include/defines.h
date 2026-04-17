#pragma once

#define USE_GET_MILLISECOND_TIMER (true)
#define SCARFNET_EMBEDDED (1)

#if SCARFNET_EMBEDDED
//#define FASTLED_INTERNAL (1)
#include <FastLED.h>
#endif

#include <string>
#include <vector>

const int kLedPin  = 26; // This pin is ignored when using FASTLED_ESP8266_DMA
const int kNumLeds = 25;

const int kBuiltinLedPin = 27; // GPIO number of builtin LED
const int kNumBuiltinLeds = 1;

const int kButtonPin = 39;

enum ELedType: int {
    kLedType_Adafruit = 0,
    kLedType_Amazon = 1,

    kLedType_Count,
};

template<uint8_t DATA_PIN> class ADAFRUIT : public WS2812Controller800Khz<DATA_PIN, GRB> {};
template<uint8_t DATA_PIN> class AMAZON : public WS2812Controller800Khz<DATA_PIN, RGB> {};
template<uint8_t DATA_PIN> class M5_INTERNAL_TYPE : public WS2812Controller800Khz<DATA_PIN, GRB> {};

//#define LED_TYPE ADAFRUIT
#define LED_TYPE AMAZON

#include "typedefs.h"
