#pragma once

#include <painlessMesh.h>
#include <Arduino.h>
#include <M5Stack.h>
#include <FastLED.h>

class Scarf
{
public:
    void setup();
    void loop();

    void showBuiltInLED();
    void showLEDs();
};

void showLEDs();
