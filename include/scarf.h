#pragma once

#include <painlessMesh.h>
#include <Arduino.h>
#include <M5Stack.h>
#include <FastLED.h>

class Scarf
{
public:
    Scarf();
    
    void setup();
    void loop();

    void showBuiltInLED();
    void showLEDs();

private:
    void        updateTime();

    uint32_t    _timeUsec {0};
    uint32_t    _timeSec{0};
    uint32_t    _usecPeriod{1000000};
};

void    showLEDs();
int     getNumNodes();
