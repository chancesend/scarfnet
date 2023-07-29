#pragma once

#include "Mesh.h"
#include "ObservableButton.h"
#include "patterns.h"

#include "defines.h"

#include <Arduino.h>
#include <FastLED.h>
#include <M5Stack.h>
#include <Preferences.h>

#include <memory>
#include <string>

namespace Scarf
{

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

class Scarf
{
public:
    Scarf();

    void setup();
    void loop();

    void showLEDs();

private:
    const int kNumBuiltinLeds = 1;

    const int kBlinkPeriodMs = 3000; // milliseconds until cycle repeat

    const int kButtonPin = 39;

    // Prototypes

    void showBuiltInLED();
    void updateTime();
    void initMesh();
    void watchdog();
    
    int  getNumNodes();

    void sendMessage();
    void currentPatternRun();
    void blinkNumNodes();
    void onConnectionChange();
    void onReceivedData(const DynamicJsonDocument& doc);

    void initPatterns();
    void incrementPattern();
    void changePatternFromString(const std::string& pattern, Rnd randomizer);

    Mesh::TimeMs     _timeMsec {0};
    uint32_t    _timeSec{0};
    uint32_t    _msecPeriod{2300};
    
    ObservableButton      _nextPatternButton;
    Mesh::TimeMs _lastSelfButtonPressMs {0};
    Mesh::TimeMs _lastAnyMeshPressMs {0};
    bool        _onFlag {false};

    Preferences preferences;
    
    Task _taskCurrentPatternRun;
    Task _taskSendMessage; // start with a one second interval
    // Task to blink the number of nodes
    Task _blinkNoNodes;

    Leds        _leds;
    Leds        _ledsReal;

    Leds        _builtinLED;
    Scheduler       _userScheduler; // to control your personal task
    
    std::unique_ptr<Mesh>   _mesh;

    PatternList _patterns;
    PatternList::iterator _currentPattern;
    Rnd _currentRandomizer {0};
};

}
