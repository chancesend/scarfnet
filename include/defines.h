#pragma once

#include <FastLED.h>
#include <vector>

#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

#define LED_PIN     26 // This pin is ignorred when using FASTLED_ESP8266_DMA
#define NUM_LEDS    25
#define BRIGHTNESS  100 // Range 0 - 255
#define LED_TYPE    NEOPIXEL
#define COLOR_ORDER GRB

typedef std::vector<CRGB> led_list;
