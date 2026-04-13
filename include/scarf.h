#pragma once

#include "Mesh.h"
#include "ObservableButton.h"
#include "OtaManager.h"
#include "patterns.h"
#include "PatternManager.h"

#include "defines.h"

#include <Arduino.h>
#include <FastLED.h>
#include <M5Stack.h>
#include <Preferences.h>

#include <memory>
#include <string>
// No mutex needed: painlessMesh callbacks fire synchronously from within
// mesh.update() on the same core/task as loop(), so all state access is
// single-threaded via cooperative scheduling (TaskScheduler).

namespace Scarfnet
{

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// Top-level coordinator. Wires together Mesh, ObservableButton, PatternManager,
// and OtaManager. Call setup() once, then loop() or otaLoop() every iteration
// depending on isInOtaMode().
class Scarf
{
public:
    Scarf();

    // Initializes all subsystems. If OTA mode is triggered at boot, returns
    // immediately — mesh and pattern setup are skipped.
    void setup();
    // Normal operation loop: drives the mesh, renders LEDs, and blends palettes.
    void loop();
    // OTA operation loop: services the OtaManager while mesh is inactive.
    void otaLoop();

    // Returns true when OTA mode was triggered at boot.
    bool isInOtaMode() const { return _otaManager && _otaManager->isActive(); }
    Mesh::TimeMs getTimeMsec() { return this->_timeMsec; }

private:
    // Prototypes

    void showLEDs();
    void showBuiltInLED();
    void updateTime();
    void processEvent(const ObservableButton::Event &event);

    void sendMessage();
    void blinkNumNodes();
    void onConnectionChange();
    void onReceivedData(const JsonDocument& doc);

    Mesh::TimeMs     _timeMsec {0};
    
    ObservableButton      _nextPatternButton;
    Mesh::TimeMs _lastSelfButtonPressMs {0};
    bool        _onFlag {false};

    Preferences _preferences;
    
    Task _taskSendMessage;
    // Task to blink the number of nodes
    Task _blinkNoNodes;
    Task _taskLogMemory;

    uint32_t    _changeIndex {0};

    Leds        _ledsReal;

    Leds        _builtinLED;
    Scheduler       _userScheduler; // to control your personal task
    
    std::unique_ptr<Mesh>       _mesh;
    std::unique_ptr<OtaManager> _otaManager;

    PatternManager::Ptr  _patternManager;
};

}
