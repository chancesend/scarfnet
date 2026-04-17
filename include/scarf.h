#pragma once

#include "defines.h"
#include "sync.h"
#include "Mesh.h"
#include "ObservableButton.h"
#include "OtaManager.h"
#include "patterns.h"
#include "PatternManager.h"

#include <TaskScheduler.h>
#include <Arduino.h>
#include <FastLED.h>
#include <M5Stack.h>
#include <Preferences.h>

#include <memory>
#include <string>

// ESP-NOW callbacks fire on the WiFi task and are queued in Mesh's ring buffer.
// By the time any callback reaches Scarf (via _receivedCb / _joinedCb / _leftCb),
// it has already been dispatched on the loop task inside Mesh::update(). All Scarf
// state access is therefore single-threaded.

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

    void setup();
    void loop();
    void otaLoop();

    bool           isInOtaMode()  const { return _otaManager && _otaManager->isActive(); }
    Mesh::TimeMs   getTimeMsec()  const { return _timeMsec; }

private:
    void showLEDs();
    void showBuiltInLED();
    void updateTime();
    void processEvent(const ObservableButton::Event& event);

    void sendMessage();
    void blinkNumNodes();
    void onNodeJoined(Mesh::NodeId nodeId);
    void onNodeLeft(Mesh::NodeId nodeId);
    void onReceived(const HeartbeatPacket& pkt);

    Mesh::TimeMs _timeMsec                  {0};

    ObservableButton _nextPatternButton;
    Mesh::TimeMs     _lastSelfButtonPressMs {0};
    bool             _onFlag        {false};

    Preferences _preferences;

    Task _taskSendMessage;
    Task _blinkNoNodes;
    Task _taskLogMemory;
    // Fires kBurstSyncCount times at kBurstSyncIntervalMs after a node joins,
    // so the new peer converges pattern and clock quickly.
    Task _taskBurstSync;

    ChangeIndex _changeIndex {0};

    Leds _ledsReal;
    Leds _builtinLED;

    Scheduler _userScheduler;

    std::unique_ptr<Mesh>       _mesh;
    std::unique_ptr<OtaManager> _otaManager;

    PatternManager::Ptr _patternManager;
};

}
