#include "scarf.h"

#include "config.h"
#include "defines.h"
#include "patterns.h"
#include "palettes.h"
#include "log.h"
#include "sync.h"
#include "sync_state.h"
#include "builtin_led.h"
#include "heartbeat_builder.h"

namespace Scarfnet
{

// Persisted in NVS under the "scarfNet" namespace and survives USB/OTA firmware
// updates. Renaming the key or namespace silently resets every scarf's LED type.
static const char* const kLedTypeString = "ledType";

Scarf::Scarf() :
    _patternManager(std::make_shared<Scarfnet::PatternManager>()),
    _nextPatternButton(&_userScheduler, kButtonPin),
    _taskSendMessage(kHeartbeatIntervalMs, TASK_FOREVER,
        [this]() { sendMessage(); }),
    _taskLogMemory(kMemLogIntervalMs, TASK_FOREVER,
        [this]()
        {
            Scarfnet::log("[MEM] free: %u  min-free: %u",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
        }),
    _taskBurstSync(kBurstSyncIntervalMs, TASK_ONCE,
        [this]() { sendMessage(); }),
    _taskRebroadcast(0, TASK_ONCE,
        [this]() { sendMessage(); })
{
    Scarfnet::log("Scarf::Scarf()");
}

void Scarf::sendMessage()
{
    HeartbeatPacket pkt = buildHeartbeat(_mesh->nodeId(), _mesh->timeMs(), _sync,
                                         _patternManager->getCurrentPattern(), _tempo);
    _mesh->broadcast(pkt);

    // Jitter the next interval so nodes on identical boot schedules drift apart.
    int32_t jitter = (int32_t)random(kHeartbeatJitterMs * 2 + 1) - (int32_t)kHeartbeatJitterMs;
    _taskSendMessage.setInterval(kHeartbeatIntervalMs + jitter);
    Scarfnet::log("[SND] id=%u pattern=%s ci=%u time=%u rnd=%u ti=%u beat=%u/%u ver=%u",
                  pkt.id, pkt.pattern, pkt.changeIndex, pkt.currentTimeMs,
                  pkt.globalRandomizer, pkt.tempoIndex, pkt.beatIntervalMs, pkt.beatPhaseMs,
                  pkt.version);
}

void Scarf::onNodeJoined(Mesh::NodeId nodeId)
{
    Scarfnet::log("[SCARF] node %u joined (%u peers)", nodeId, (unsigned)_mesh->nodeCount());
    _blinkNoNodes.setIterations((int)_mesh->nodeCount() * 2);
    TimeMs msToNextSync = kNodeBlinkPeriodMs - (_mesh->timeMs() % kNodeBlinkPeriodMs);
    _blinkNoNodes.enableDelayed(msToNextSync);

    // Burst sync so the joining node converges pattern and clock quickly.
    // Every peer hears the join at the same moment; jitter the start so their
    // bursts don't collide on air. See kBurstSyncJitterMs.
    _taskBurstSync.setIterations(kBurstSyncCount);
    _taskBurstSync.enableDelayed(kBurstSyncIntervalMs + (unsigned long)random(kBurstSyncJitterMs + 1));
}

void Scarf::onNodeLeft(Mesh::NodeId nodeId)
{
    Scarfnet::log("[SCARF] node %u left (%u peers)", nodeId, (unsigned)_mesh->nodeCount());
    _blinkNoNodes.setIterations((int)_mesh->nodeCount() * 2);
}

void Scarf::debugModeUpdate(const HeartbeatPacket& pkt) {
    // Record this heartbeat for the debug pattern flash effect.
    // Find existing entry for this node and update it, or add/replace.
    bool found = false;
    for (int i = 0; i < _nodeFlashCount; i++) {
        if (_nodeFlashes[i].id == pkt.id) {
            _nodeFlashes[i].when = _timeMsec;
            found = true;
            break;
        }
    }
    if (!found) {
        if (_nodeFlashCount < kMaxNodeFlashes) {
            _nodeFlashes[_nodeFlashCount++] = {pkt.id, _timeMsec};
        } else {
            // Replace oldest entry.
            int oldest = 0;
            for (int i = 1; i < kMaxNodeFlashes; i++)
                if (_nodeFlashes[i].when < _nodeFlashes[oldest].when) oldest = i;
            _nodeFlashes[oldest] = {pkt.id, _timeMsec};
        }
    }
}

void Scarf::onReceived(const HeartbeatPacket& pkt)
{
    if (pkt.version > kScarfVersion)
        _newerVersionSeen = true;

    this->debugModeUpdate(pkt);

    // Tempo is versioned independently of the pattern: adopt only a newer one.
    switch (_tempo.receive(pkt.tempoIndex, pkt.beatIntervalMs, pkt.currentTimeMs, pkt.beatPhaseMs)) {
        case TempoSync::Result::Ignored:
            break;
        case TempoSync::Result::TakenOver:
            Scarfnet::log("[SCARF][TEMPO] node %u took over tap-tempo (ti=%u interval=%u) — leaving tap mode",
                          pkt.id, pkt.tempoIndex, pkt.beatIntervalMs);
            _taskRebroadcast.restartDelayed((unsigned long)random(kRebroadcastJitterMs + 1));
            break;
        case TempoSync::Result::Adopted:
            Scarfnet::log("[SCARF][TEMPO] adopting ti=%u interval=%u from node %u",
                          pkt.tempoIndex, pkt.beatIntervalMs, pkt.id);
            _taskRebroadcast.restartDelayed((unsigned long)random(kRebroadcastJitterMs + 1));
            break;
    }

    const std::string currentPattern = _patternManager->getCurrentPattern();
    switch (classifyUpdate(pkt, _sync, currentPattern.c_str())) {
        case UpdateDecision::Accept:
            break;
        case UpdateDecision::Ignore:
            return;
        case UpdateDecision::RejectDiffering:
            Scarfnet::log("[SCARF][RCV] STUCK? rejected differing pattern=%s from node %u — "
                          "incoming (ci=%u rnd=%u by=%u) not newer than local "
                          "(ci=%u rnd=%u by=%u pattern=%s)",
                          pkt.pattern, pkt.id,
                          pkt.changeIndex, pkt.globalRandomizer, pkt.lastPressId,
                          _sync.changeIndex, _sync.randomizer, _sync.lastPressId,
                          currentPattern.c_str());
            return;
    }

    Scarfnet::log("[SCARF][RCV] accepting pattern=%s randomizer=%u ci=%u from node %u",
                  pkt.pattern, pkt.globalRandomizer, pkt.changeIndex, pkt.id);
    if (!_patternManager->changePatternFromString(pkt.pattern, pkt.globalRandomizer)) {
        // Unknown pattern name — this scarf's registry doesn't have it, likely a
        // firmware/pattern-set mismatch. Don't advance the change index, so every
        // later heartbeat retries (and re-flashes red) until a known pattern wins.
        Scarfnet::log("[SCARF][RCV] REJECTED unknown pattern=%s from node %u ver=%u (ours ver=%u)",
                      pkt.pattern, pkt.id, pkt.version, kScarfVersion);
        _unknownPatternAlarm.trigger(millis());
        return;
    }

    // Adopts the randomizer too, so our heartbeats relay the presser's variation
    // rather than our own stale one. See sync_state.h.
    adoptSyncState(_sync, pkt);

    // Epidemic re-broadcast: we just accepted a new state, so push it onward
    // immediately (after a little jitter) rather than waiting for the next
    // scheduled heartbeat. Nodes that already have it will reject the duplicate,
    // so each node re-emits at most once per update. See kRebroadcastJitterMs.
    _taskRebroadcast.restartDelayed((unsigned long)random(kRebroadcastJitterMs + 1));
}

void Scarf::setup()
{
    Scarfnet::log("Scarf::setup()");

    _builtinLED.resize(kNumBuiltinLeds);
    _builtinLedController = &FastLED.addLeds<M5_INTERNAL_TYPE, kBuiltinLedPin>(_builtinLED.data(), _builtinLED.size());

    _otaManager = make_unique<OtaManager>(kButtonPin, [this](CRGB color)
    {
        for (auto& led : _builtinLED) { led = color; }
        FastLED.show();
    });
    if (_otaManager->checkBootTrigger())
        return;

    _preferences.begin("scarfNet", false);
    bool isLedTypeSet = _preferences.isKey(kLedTypeString);
    if (!isLedTypeSet) {
        _preferences.putInt(kLedTypeString, kLedType_Amazon);
        Scarfnet::log("LED type not set, defaulting to %i", kLedType_Amazon);
    }
    int ledType = _preferences.getInt(kLedTypeString, kLedType_Amazon);
    static const char* const kLedTypeNames[] = { "Adafruit", "Amazon", "Bike" };
    const char* ledTypeName = (ledType >= 0 && ledType < kLedType_Count)
                              ? kLedTypeNames[ledType] : "Unknown";
    Scarfnet::log("LED type %i (%s) loaded", ledType, ledTypeName);

    _mesh = make_unique<Mesh>();
    _mesh->begin();
    _mesh->onReceived([this](const HeartbeatPacket& pkt) { onReceived(pkt); });
    _mesh->onNodeJoined([this](NodeId id) { onNodeJoined(id); });
    _mesh->onNodeLeft( [this](NodeId id) { onNodeLeft(id); });

    _nextPatternButton.setup();
    _nextPatternButton.addObserver([this](const ObservableButton::Event& event)
                                   { processEvent(event); });

    _ledsReal.resize(kNumLeds);
    switch (ledType) {
        case kLedType_Adafruit:
            FastLED.addLeds<ADAFRUIT, kLedPin>(_ledsReal.data(), _ledsReal.size());
            break;
        case kLedType_Bike:
            FastLED.addLeds<BIKE, kLedPin>(_ledsReal.data(), _ledsReal.size());
            break;
        case kLedType_Amazon:
        default:
            FastLED.addLeds<AMAZON, kLedPin>(_ledsReal.data(), _ledsReal.size());
            break;
    }

    FastLED.setMaxPowerInMilliWatts(500);

    _userScheduler.addTask(_taskSendMessage);
    _taskSendMessage.enable();

    _userScheduler.addTask(_taskLogMemory);
    _taskLogMemory.enable();

    _userScheduler.addTask(_taskBurstSync);
    _userScheduler.addTask(_taskRebroadcast);

    _blinkNoNodes.set(kNodeBlinkPeriodMs, 0,
                      [this]() { blinkNumNodes(); });
    _userScheduler.addTask(_blinkNoNodes);

    randomSeed(micros());
    Scarfnet::log("Scarf::setup() done");
}

void Scarf::processEvent(const ObservableButton::Event& event)
{
    switch (event)
    {
        case ObservableButton::Event::eDown:
        {
            if (_tempo.tapMode) {
                _tempo.tap(_mesh->timeMs());
                Scarfnet::log("eDown (tap-tempo) interval=%ums active=%d ti=%u",
                    _tempo.tempo.beatIntervalMs(), (int)_tempo.tempo.isActive(), _tempo.index);
                _taskSendMessage.forceNextIteration();
            }
            break;
        }
        case ObservableButton::Event::ePress:
        {
            if (!_tempo.tapMode) {
                _sync.lastPressMs = _mesh->timeMs();
                _sync.lastPressId = _mesh->nodeId();
                _sync.randomizer  = (Rnd)random();
                _patternManager->incrementPattern(_sync.randomizer);
                _sync.changeIndex += 1;
                Scarfnet::log("ePress → pattern=%s randomizer=%u ci=%u",
                    _patternManager->getCurrentPattern().c_str(),
                    _sync.randomizer, _sync.changeIndex);
                _taskSendMessage.forceNextIteration();
            }
            break;
        }
        case ObservableButton::Event::eLongPress:
        {
            if (kTapTempoOnLongPress) {
                if (_tempo.tapMode) _tempo.exitTapMode();
                else                _tempo.enterTapMode();
                Scarfnet::log("eLongPress → tap-tempo %s ti=%u",
                    _tempo.tapMode ? "ON" : "OFF", _tempo.index);
            } else {
                // Original behaviour: same pattern, new randomizer.
                _sync.lastPressMs = _mesh->timeMs();
                _sync.lastPressId = _mesh->nodeId();
                _sync.randomizer  = (Rnd)random();
                _patternManager->samePatternDifferentRandomizer(_sync.randomizer);
                _sync.changeIndex += 1;
                Scarfnet::log("eLongPress → pattern=%s randomizer=%u ci=%u",
                    _patternManager->getCurrentPattern().c_str(),
                    _sync.randomizer, _sync.changeIndex);
            }
            _taskSendMessage.forceNextIteration();
            break;
        }
        case ObservableButton::Event::eExtraLongPress:
        {
            auto ledType = _preferences.getInt(kLedTypeString, kLedType_Amazon);
            ledType = (ledType + 1) % kLedType_Count;
            _preferences.putInt(kLedTypeString, ledType);
            Scarfnet::log("eExtraLongPress — LED type → %i, restarting", ledType);
            delay(1000);
            ESP.restart();
            break;
        }
    }
}

void Scarf::loop()
{
    _userScheduler.execute();
    _mesh->update();
    updateTime();

    EVERY_N_MILLISECONDS(kLedRefreshRateMs)
    {
        showLEDs();
        showBuiltInLED();
        FastLED.show();
        // FastLED.show() applies global power scaling to all controllers, which
        // dims the built-in LED along with the external strip. Re-show the built-in
        // LED immediately at full brightness so it's excluded from current limiting.
        _builtinLedController->showLeds(128);
    }
    EVERY_N_MILLISECONDS(kPaletteBlendRateMs)
    {
        _patternManager->blendPalette(_timeMsec);
    }
}

void Scarf::updateTime()
{
    _timeMsec = _mesh->timeMs();
}

void Scarf::showBuiltInLED()
{
    uint32_t now = millis();
    IndicatorState state {
        .alarmActive      = _unknownPatternAlarm.isActive(now),
        .alarmOn          = _unknownPatternAlarm.isOn(now),
        .tapTempoMode     = _tempo.tapMode,
        .tempoActive      = _tempo.tempo.isActive(),
        .beatIntervalMs   = _tempo.tempo.beatIntervalMs(),
        .beatPhaseMs      = _tempo.tempo.beatPhaseMs(_timeMsec),
        .meshTimeMs       = _timeMsec,
        .newerVersionSeen = _newerVersionSeen,
    };
    CRGB color = builtinLedColor(state);
    for (auto& led : _builtinLED) led = color;
}

void Scarf::showLEDs()
{
    NodeInfo nodeInfo {
        .nodeId = _mesh->nodeId(),
        .lastPressId = _sync.lastPressId,
        .flashes = _nodeFlashes,
        .flashCount = _nodeFlashCount
    };
    
    _patternManager->runCurrentPattern(
        _ledsReal, 
        _mesh->timeMs(), 
        _tempo.tempo.beatInfo(_timeMsec),
        nodeInfo);
}

void Scarf::blinkNumNodes()
{
    constexpr int kBlinkDurationMs = 100;
    _onFlag = !_onFlag;
    _blinkNoNodes.delay(kBlinkDurationMs);

    if (_blinkNoNodes.isLastIteration())
    {
        _blinkNoNodes.setIterations((int)_mesh->nodeCount() * 2);
        TimeMs msToNextBlinkSync = kNodeBlinkPeriodMs -
            (_mesh->timeMs() % kNodeBlinkPeriodMs);
        _blinkNoNodes.enableDelayed(msToNextBlinkSync);
    }
}

void Scarf::otaLoop()
{
    _otaManager->loop();
}

} // namespace Scarfnet
