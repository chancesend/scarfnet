#pragma once

#include "defines.h"
#include "sync_controller.h"
#include "builtin_led.h"
#include "Mesh.h"
#include "ObservableButton.h"
#include "OtaManager.h"
#include "patterns.h"
#include "PatternManager.h"

#include <TaskScheduler.h>
#include <Arduino.h>
#include <FastLED.h>
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

// Top-level coordinator: wires Mesh, button, patterns and OTA together. Call
// setup() once, then loop() or otaLoop() every iteration per isInOtaMode().
class Scarf
{
public:
    Scarf();

    void setup();
    void loop();
    void otaLoop();

    bool isInOtaMode() const { return _otaManager && _otaManager->isActive(); }

private:
    void showLEDs();
    void showBuiltInLED();
    void updateTime();
    void processEvent(const ObservableButton::Event& event);

    void sendMessage();
    void scheduleRebroadcast();
    Rnd  recordLocalPress();
    void onNodeJoined(Mesh::NodeId nodeId);
    void onNodeLeft(Mesh::NodeId nodeId);
    void onReceived(const HeartbeatPacket& pkt);

    Mesh::TimeMs _timeMsec                  {0};

    ObservableButton _nextPatternButton;

    // Timed on local millis(), so a mesh clock jump can't cut it short.
    FlashBurst _unknownPatternAlarm {kUnknownPatternFlashCount, kUnknownPatternFlashHalfPeriodMs};

    NodeFlashTable _nodeFlashes;

    Preferences _preferences;

    Task _taskSendMessage;
    Task _taskLogMemory;
    Task _taskBurstSync;
    Task _taskRebroadcast;

    SyncController _sync;

    Leds _ledsReal;
    Leds _builtinLED;
    // Kept to re-show the built-in LED after FastLED.show() power-scales it.
    CLEDController* _builtinLedController {nullptr};

    Scheduler _userScheduler;

    std::unique_ptr<Mesh>       _mesh;
    std::unique_ptr<OtaManager> _otaManager;

    PatternManager::Ptr _patternManager;
};

}
