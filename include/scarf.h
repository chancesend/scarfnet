#pragma once

#include "Mesh.h"
#include "defines.h"

#include <Arduino.h>
#include <M5Stack.h>
#include <FastLED.h>

class Button
{
public:
    enum ButtonState
    {
        kButtonState_Up,
        kButtonState_Pressed,
        kButtonState_LongPressed,
        kButtonState_DoublePressed
    };

    Button();
private:
    ButtonState buttonState = kButtonState_Up;
    int32_t lastSelfButtonPress = 0;
}

class Scarf : public Mesh::ConnectionObserver
{
public:
    Scarf();

    void setup();
    void loop();

    void showLEDs();

    // Mesh::ConnectionObserver overrides
    void onConnectionChange() override;

private:
    const int kNumBuiltinLeds = 1;

    const int kBlinkPeriod = 3000; // milliseconds until cycle repeat

    const int kButtonPin = 39;

    // Prototypes

    Button _button;

    void showBuiltInLED();
    void        updateTime();
    void        initMesh();
    
    int         getNumNodes();

    void checkButtonPress();
    void sendMessage();
    void currentPatternRun();
    void blinkNumNodes();

    uint32_t    _timeUsec {0};
    uint32_t    _timeSec{0};
    uint32_t    _usecPeriod{1000000};
    
    Button      _nextPatternButton;
    bool        _onFlag {false};
    
    Task _taskCheckButtonPress;
    Task _taskCurrentPatternRun;
    Task _taskSendMessage; // start with a one second interval
    // Task to blink the number of nodes
    Task _blinkNoNodes;

    led_list        _leds;
    led_list        _ledsReal;

    led_list        _builtinLED;
    Scheduler       _userScheduler; // to control your personal task
    
    Mesh            _mesh;
};

