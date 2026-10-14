#pragma once

#include "defines.h"
#include "sync.h"
#include "sync_state.h"
#include "Mesh.h"
#include "ObservableButton.h"
#include "OtaManager.h"
#include "patterns.h"
#include "PatternManager.h"
#include "tap_tempo.h"

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
    void debugModeUpdate(const HeartbeatPacket& pkt);

    Mesh::TimeMs _timeMsec                  {0};

    ObservableButton _nextPatternButton;
    bool             _onFlag        {false};

    TapTempo _tapTempo;
    bool     _tapTempoMode     {false};  // true = long-press entered tap-tempo input mode
    TimeMs   _lastLocalTapMs   {0};     // mesh time of the most recent local tap
    bool     _newerVersionSeen {false};  // true if any peer has a higher firmware version
    // Set when a peer tells us to switch to a pattern we don't have; drives a
    // burst of red flashes on the built-in LED. Start time is local millis().
    bool     _unknownPatternFlashActive  {false};
    uint32_t _unknownPatternFlashStartMs {0};

    NodeFlash _nodeFlashes[kMaxNodeFlashes] {};
    int       _nodeFlashCount {0};

    Preferences _preferences;

    Task _taskSendMessage;
    Task _blinkNoNodes;
    Task _taskLogMemory;
    // Fires kBurstSyncCount times at kBurstSyncIntervalMs after a node joins,
    // so the new peer converges pattern and clock quickly.
    Task _taskBurstSync;
    // One-shot: re-broadcasts a freshly accepted update after a short random
    // delay so the change floods epidemically instead of trickling. See
    // kRebroadcastJitterMs.
    Task _taskRebroadcast;

    // Replicated pattern-change state (changeIndex, last press, randomizer)
    // carried in every heartbeat. See sync_state.h.
    SyncState _sync {};

    Leds _ledsReal;
    Leds _builtinLED;
    // Pointer to the built-in LED's FastLED controller so we can re-show it at
    // full brightness after FastLED.show() applies global power scaling.
    CLEDController* _builtinLedController {nullptr};

    Scheduler _userScheduler;

    std::unique_ptr<Mesh>       _mesh;
    std::unique_ptr<OtaManager> _otaManager;

    PatternManager::Ptr _patternManager;
};

}
