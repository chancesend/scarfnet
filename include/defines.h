#pragma once

#define FASTLED_INTERNAL (1)
#include <FastLED.h>
#include <painlessMesh.h>

#include <string>
#include <vector>

const std::string kMeshSSID = "scarfNet";
const std::string kMeshPassword = "aNetOfScarves";
const uint16_t kMeshPort = 5555;

const int kLedPin = 26; // This pin is ignored when using FASTLED_ESP8266_DMA
const int kNumLeds= 25;
const int kBrightness = 100; // Range 0 - 255

#define LED_TYPE    NEOPIXEL
const EOrder kColorOrder = GRB;

typedef std::vector<CRGB> led_list;
