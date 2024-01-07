#pragma once

#define USE_GET_MILLISECOND_TIMER (true)
#define SCARFNET_EMBEDDED (1)

#if SCARFNET_EMBEDDED
//#define FASTLED_INTERNAL (1)
#include <FastLED.h>
#endif

#include <painlessMesh.h>

#include <string>
#include <vector>

const std::string kMeshSSID = "scarfNet";
const std::string kMeshPassword = "aNetOfScarves";
const uint16_t kMeshPort = 5555;

const int kLedPin  = 26; // This pin is ignored when using FASTLED_ESP8266_DMA
const int kNumLeds = 25;

const int kBuiltinLedPin = 27; // GPIO number of builtin LED
const int kNumBuiltinLeds = 1;

const int kButtonPin = 39;

template<uint8_t DATA_PIN> class ADAFRUIT : public WS2812Controller800Khz<DATA_PIN, GRB> {};
template<uint8_t DATA_PIN> class AMAZON : public WS2812Controller800Khz<DATA_PIN, RGB> {};
template<uint8_t DATA_PIN> class M5_INTERNAL_TYPE : public WS2812Controller800Khz<DATA_PIN, GRB> {};

#define LED_TYPE AMAZON

typedef std::vector<CRGB> Leds;
typedef uint8_t Rnd;
