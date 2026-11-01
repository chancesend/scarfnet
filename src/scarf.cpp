#include "scarf.h"

#include "config.h"
#include "defines.h"
#include "patterns.h"
#include "palettes.h"
#include "log.h"
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
    HeartbeatPacket pkt = buildHeartbeat(_mesh->nodeId(), _mesh->timeMs(), _sync.state(),
                                         _patternManager->getCurrentPattern(), _sync.tempo());
    _mesh->broadcast(pkt);

    // Jitter the next interval so nodes on identical boot schedules drift apart.
    TimeDiffMs jitter = (TimeDiffMs)random(kHeartbeatJitterMs * 2 + 1) - (TimeDiffMs)kHeartbeatJitterMs;
    _taskSendMessage.setInterval(kHeartbeatIntervalMs + jitter);
    if (kVerboseLogging)
        Scarfnet::log("[SND] id=%u pattern=%s ci=%u time=%u rnd=%u ti=%u beat=%u/%u ver=%u",
                      pkt.id, pkt.pattern, pkt.changeIndex, pkt.currentTimeMs,
                      pkt.globalRandomizer, pkt.tempoIndex, pkt.beatIntervalMs, pkt.beatPhaseMs,
                      pkt.version);
}

void Scarf::onNodeJoined(Mesh::NodeId nodeId)
{
    Scarfnet::log("[SCARF] node %u joined (%u peers)", nodeId, (unsigned)_mesh->nodeCount());
    _taskBurstSync.setIterations(kBurstSyncCount);
    _taskBurstSync.enableDelayed(kBurstSyncIntervalMs + (unsigned long)random(kBurstSyncJitterMs + 1));
}

void Scarf::onNodeLeft(Mesh::NodeId nodeId)
{
    Scarfnet::log("[SCARF] node %u left (%u peers)", nodeId, (unsigned)_mesh->nodeCount());
}

// Push a freshly accepted state onward without waiting for the next heartbeat.
void Scarf::scheduleRebroadcast()
{
    _taskRebroadcast.restartDelayed((unsigned long)random(kRebroadcastJitterMs + 1));
}

void Scarf::onReceived(const HeartbeatPacket& pkt)
{
    _nodeFlashes.record(pkt.id, _timeMsec);

    const std::string currentPattern = _patternManager->getCurrentPattern();
    const auto r = _sync.receive(pkt, currentPattern.c_str(), [this](const char* pattern, Rnd randomizer) {
        return _patternManager->changePatternFromString(pattern, randomizer);
    });
    const SyncState& local = _sync.state();

    switch (r.tempo) {
        case TempoSync::Result::Ignored:
            break;
        case TempoSync::Result::TakenOver:
            Scarfnet::log("[SCARF][TEMPO] node %u took over tap-tempo (ti=%u interval=%u) — leaving tap mode",
                          pkt.id, pkt.tempoIndex, pkt.beatIntervalMs);
            break;
        case TempoSync::Result::Adopted:
            Scarfnet::log("[SCARF][TEMPO] adopting ti=%u interval=%u from node %u",
                          pkt.tempoIndex, pkt.beatIntervalMs, pkt.id);
            break;
    }

    switch (r.decision) {
        case UpdateDecision::Ignore:
            break;
        case UpdateDecision::RejectDiffering:
            Scarfnet::log("[SCARF][RCV] STUCK? rejected differing pattern=%s from node %u — "
                          "incoming (ci=%u rnd=%u by=%u) not newer than local "
                          "(ci=%u rnd=%u by=%u pattern=%s)",
                          pkt.pattern, pkt.id,
                          pkt.changeIndex, pkt.globalRandomizer, pkt.lastPressId,
                          local.changeIndex, local.randomizer, local.lastPressId,
                          currentPattern.c_str());
            break;
        case UpdateDecision::Accept:
            if (r.unknownPattern) {
                Scarfnet::log("[SCARF][RCV] REJECTED unknown pattern=%s from node %u ver=%u (ours ver=%u)",
                              pkt.pattern, pkt.id, pkt.version, kScarfVersion);
                _unknownPatternAlarm.trigger(millis());
            } else {
                Scarfnet::log("[SCARF][RCV] accepted pattern=%s randomizer=%u ci=%u from node %u",
                              pkt.pattern, pkt.globalRandomizer, pkt.changeIndex, pkt.id);
            }
            break;
    }

    if (r.rebroadcast)
        scheduleRebroadcast();
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

    FastLED.setMaxPowerInMilliWatts(kLedStripPowerLimitMw);

    _userScheduler.addTask(_taskSendMessage);
    _taskSendMessage.enable();

    _userScheduler.addTask(_taskLogMemory);
    _taskLogMemory.enable();

    _userScheduler.addTask(_taskBurstSync);
    _userScheduler.addTask(_taskRebroadcast);

    // FastLED's generator starts identically on every board; seed it now that
    // the radio is on and esp_random() draws real entropy.
    random16_set_seed((uint16_t)esp_random());
    Scarfnet::log("Scarf::setup() done");
}

// A local press that changes the look: claim the next change index with a
// fresh randomizer. Returns the randomizer to apply.
Rnd Scarf::recordLocalPress()
{
    Rnd randomizer = (Rnd)esp_random();
    _sync.localPress(_mesh->timeMs(), _mesh->nodeId(), randomizer);
    Scarfnet::log("[SCARF] local press → randomizer=%u ci=%u", randomizer, _sync.state().changeIndex);
    return randomizer;
}

void Scarf::processEvent(const ObservableButton::Event& event)
{
    switch (event)
    {
        case ObservableButton::Event::eDown:
        {
            if (_sync.tempo().tapMode) {
                _sync.tap(_mesh->timeMs());
                const TempoSync& t = _sync.tempo();
                Scarfnet::log("eDown (tap-tempo) interval=%ums active=%d ti=%u",
                    t.tempo.beatIntervalMs(), (int)t.tempo.isActive(), t.index);
                _taskSendMessage.forceNextIteration();
            }
            break;
        }
        case ObservableButton::Event::ePress:
        {
            if (!_sync.tempo().tapMode) {
                _patternManager->incrementPattern(recordLocalPress());
                _taskSendMessage.forceNextIteration();
            }
            break;
        }
        case ObservableButton::Event::eLongPress:
        {
            if (kTapTempoOnLongPress) {
                _sync.toggleTapMode();
                Scarfnet::log("eLongPress → tap-tempo %s ti=%u",
                    _sync.tempo().tapMode ? "ON" : "OFF", _sync.tempo().index);
            } else {
                _patternManager->samePatternDifferentRandomizer(recordLocalPress());
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
            delay(kLedTypeRestartDelayMs);
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
    _otaManager->autoUpdateTick(_sync.newerVersionSeen());

    EVERY_N_MILLISECONDS(kLedRefreshRateMs)
    {
        showLEDs();
        showBuiltInLED();
        FastLED.show();
        // FastLED.show() power-scales every controller, built-in LED included;
        // re-show the built-in LED so it is exempt from the strip's power limit.
        _builtinLedController->showLeds(kBuiltinLedBrightness);
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
    const TempoSync& tempo = _sync.tempo();
    IndicatorState state {
        .alarmActive      = _unknownPatternAlarm.isActive(now),
        .alarmOn          = _unknownPatternAlarm.isOn(now),
        .tapTempoMode     = tempo.tapMode,
        .tempoActive      = tempo.tempo.isActive(),
        .beatIntervalMs   = tempo.tempo.beatIntervalMs(),
        .beatPhaseMs      = tempo.tempo.beatPhaseMs(_timeMsec),
        .meshTimeMs       = _timeMsec,
        .newerVersionSeen = _sync.newerVersionSeen(),
    };
    CRGB color = builtinLedColor(state);
    for (auto& led : _builtinLED) led = color;
}

void Scarf::showLEDs()
{
    NodeInfo nodeInfo {
        .nodeId = _mesh->nodeId(),
        .lastPressId = _sync.state().lastPressId,
        .flashes = _nodeFlashes.entries,
        .flashCount = _nodeFlashes.count
    };
    
    _patternManager->runCurrentPattern(
        _ledsReal, 
        _mesh->timeMs(), 
        _sync.tempo().tempo.beatInfo(_timeMsec),
        nodeInfo);
}

void Scarf::otaLoop()
{
    _otaManager->loop();
}

} // namespace Scarfnet
