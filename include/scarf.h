#pragma once

#include "Mesh.h"
#include "ObservableButton.h"

#include "defines.h"

#include <Arduino.h>
#include <M5Stack.h>
#include <FastLED.h>

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

    const int kBlinkPeriod = 3000; // milliseconds until cycle repeat

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

    void initPatterns();
    void changePattern();

    uint32_t    _timeUsec {0};
    uint32_t    _timeSec{0};
    uint32_t    _usecPeriod{1000000};
    
    ObservableButton      _nextPatternButton;
    int32_t _lastSelfButtonPress = 0;
    int32_t _lastAnyMeshPress = 0;
    bool        _onFlag {false};
    
    Task _taskCurrentPatternRun;
    Task _taskSendMessage; // start with a one second interval
    // Task to blink the number of nodes
    Task _blinkNoNodes;

    led_list        _leds;
    led_list        _ledsReal;

    led_list        _builtinLED;
    Scheduler       _userScheduler; // to control your personal task
    
    std::unique_ptr<Mesh>   _mesh;

    typedef std::function<void(led_list&, int32_t)> PatternFcn;
    typedef std::pair<std::string, PatternFcn> NamedPattern;
    typedef std::vector<NamedPattern> PatternList;
    PatternList _patterns;
    PatternList::iterator _currentPattern;
};

}
