#pragma once

#include "Mesh.h"
#include "ObservableButton.h"
#include "patterns.h"
#include "PatternManager.h"

#include "defines.h"

#include <Arduino.h>
#include <FastLED.h>
#include <M5Stack.h>
#include <Preferences.h>

#include <memory>
#include <string>
//#include <mutex>

namespace Scarfnet
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

    Mesh::TimeMs getTimeMsec() { return this->_timeMsec; }

private:
    const int kBlinkPeriodMs = 3000; // milliseconds until cycle repeat

    // Prototypes

    void showLEDs();
    void showBuiltInLED();
    void updateTime();
    void initMesh();
    void watchdog();
    void processEvent(const ObservableButton::Event &event);
    
    int  getNumNodes();

    void sendMessage();
    void blinkNumNodes();
    void onConnectionChange();
    void onReceivedData(const DynamicJsonDocument& doc);

    Mesh::TimeMs     _timeMsec {0};
    uint32_t    _syncBlinkPeriodMs{5000};
    
    ObservableButton      _nextPatternButton;
    Mesh::TimeMs _lastSelfButtonPressMs {0};
    Mesh::TimeMs _lastAnyRemotePressMs {0};
    bool        _onFlag {false};

    Preferences _preferences;
    
    Task _taskCurrentPatternRun;
    Task _taskSendMessage; // start with a one second interval
    // Task to blink the number of nodes
    Task _blinkNoNodes;
    Task _taskLogMemory;

    uint32_t    _changeIndex {0};
 //   std::mutex  _mutex;

    Leds        _leds;
    Leds        _ledsReal;

    Leds        _builtinLED;
    Scheduler       _userScheduler; // to control your personal task
    
    std::unique_ptr<Mesh>   _mesh;

    PatternManager::Ptr  _patternManager;
};

}
