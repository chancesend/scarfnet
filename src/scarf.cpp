#include "Scarf.h"

#include "config.h"
#include "defines.h"
#include "patterns.h"
#include "palettes.h"
#include "log.h"
#include "sync.h"

namespace Scarfnet
{

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
        [this]() { sendMessage(); })
{
    Scarfnet::log("Scarf::Scarf()");
}

void Scarf::sendMessage()
{
    HeartbeatPacket pkt = {};
    pkt.id            = _mesh->nodeId();
    pkt.lastPress     = _lastSelfButtonPressMs;
    pkt.currentTimeMs = _mesh->timeMs();
    pkt.changeIndex   = _changeIndex;
    pkt.randomizer    = (uint16_t)_lastSelfButtonPressMs;
    pkt.version       = kScarfVersion;
    pkt.beatIntervalMs = _tapTempo.beatIntervalMs();
    pkt.beatPhaseMs    = _tapTempo.beatPhaseMs(pkt.currentTimeMs);

    const auto& patternName = _patternManager->getCurrentPattern();
    size_t copyLen = patternName.size() < sizeof(pkt.pattern) - 1
                     ? patternName.size()
                     : sizeof(pkt.pattern) - 1;
    memcpy(pkt.pattern, patternName.c_str(), copyLen);
    pkt.pattern[copyLen] = '\0';

    _mesh->broadcast(pkt);

    // Jitter the next interval so nodes on identical boot schedules drift apart.
    int32_t jitter = (int32_t)random(kHeartbeatJitterMs * 2 + 1) - (int32_t)kHeartbeatJitterMs;
    _taskSendMessage.setInterval(kHeartbeatIntervalMs + jitter);
    Scarfnet::log("[SND] id=%u pattern=%s ci=%u time=%u rnd=%u ver=%u",
                  pkt.id, pkt.pattern, pkt.changeIndex, pkt.currentTimeMs,
                  pkt.randomizer, pkt.version);
}

void Scarf::onNodeJoined(Mesh::NodeId nodeId)
{
    Scarfnet::log("[SCARF] node %u joined (%u peers)", nodeId, (unsigned)_mesh->nodeCount());
    _blinkNoNodes.setIterations((int)_mesh->nodeCount() * 2);
    TimeMs msToNextSync = kNodeBlinkPeriodMs - (_mesh->timeMs() % kNodeBlinkPeriodMs);
    _blinkNoNodes.enableDelayed(msToNextSync);

    // Burst sync so the joining node converges pattern and clock quickly.
    _taskBurstSync.setIterations(kBurstSyncCount);
    _taskBurstSync.enableDelayed(kBurstSyncIntervalMs);
}

void Scarf::onNodeLeft(Mesh::NodeId nodeId)
{
    Scarfnet::log("[SCARF] node %u left (%u peers)", nodeId, (unsigned)_mesh->nodeCount());
    _blinkNoNodes.setIterations((int)_mesh->nodeCount() * 2);
}

void Scarf::onReceived(const HeartbeatPacket& pkt)
{
    // Sync beat info from any scarf that has an active tap-tempo.
    // Don't overwrite our own if we're the one tapping.
    if (pkt.beatIntervalMs != 0 && !_tapTempoMode) {
        _tapTempo.setFromPacket(pkt.beatIntervalMs, pkt.currentTimeMs, pkt.beatPhaseMs);
    } else if (pkt.beatIntervalMs == 0 && !_tapTempoMode) {
        _tapTempo.reset();
    }

    if (!shouldAcceptUpdate(pkt.changeIndex, _changeIndex))
        return;

    Scarfnet::log("[SCARF][RCV] accepting pattern=%s randomizer=%u ci=%u from node %u",
                  pkt.pattern, pkt.randomizer, pkt.changeIndex, pkt.id);
    if (!_patternManager->changePatternFromString(pkt.pattern, pkt.randomizer))
        return;  // unknown pattern — don't advance change index or last-press timestamp

    _changeIndex            = rolloverGuard(pkt.changeIndex);
    _lastSelfButtonPressMs  = rolloverGuard(pkt.lastPress);
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
    Scarfnet::log("LED type %i loaded", ledType);

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
            if (_tapTempoMode) {
                _tapTempo.tap(_mesh->timeMs());
                Scarfnet::log("eDown (tap-tempo) interval=%ums active=%d",
                    _tapTempo.beatIntervalMs(), (int)_tapTempo.isActive());
                _taskSendMessage.forceNextIteration();
            }
            break;
        }
        case ObservableButton::Event::ePress:
        {
            if (!_tapTempoMode) {
                _lastSelfButtonPressMs = _mesh->timeMs();
                _patternManager->incrementPattern(_lastSelfButtonPressMs);
                _changeIndex += 1;
                Scarfnet::log("ePress → pattern=%s randomizer=%u ci=%u",
                    _patternManager->getCurrentPattern().c_str(),
                    (uint16_t)_lastSelfButtonPressMs, _changeIndex);
                _taskSendMessage.forceNextIteration();
            }
            break;
        }
        case ObservableButton::Event::eLongPress:
        {
            if (kTapTempoOnLongPress) {
                _tapTempoMode = !_tapTempoMode;
                if (!_tapTempoMode) _tapTempo.reset();
                Scarfnet::log("eLongPress → tap-tempo %s", _tapTempoMode ? "ON" : "OFF");
            } else {
                // Original behaviour: same pattern, new randomizer.
                _lastSelfButtonPressMs = _mesh->timeMs();
                _patternManager->samePatternDifferentRandomizer(_lastSelfButtonPressMs);
                _changeIndex += 1;
                Scarfnet::log("eLongPress → pattern=%s randomizer=%u ci=%u",
                    _patternManager->getCurrentPattern().c_str(),
                    (uint16_t)_lastSelfButtonPressMs, _changeIndex);
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
        _patternManager->blendPalette();
    }
}

void Scarf::updateTime()
{
    _timeMsec = _mesh->timeMs();
}

void Scarf::showBuiltInLED()
{
    for (int i = 0; i < (int)_builtinLED.size(); ++i)
    {
        _builtinLED[i] = CRGB::Black;

        if (_tapTempoMode && _tapTempo.isActive()) {
            // Green pulse at the beat rate — confirms the tempo is locked in.
            uint16_t phase    = _tapTempo.beatPhaseMs(_timeMsec);
            uint16_t interval = _tapTempo.beatIntervalMs();
            if (interval > 0 && phase < interval / 10)  // ~10% duty cycle
                _builtinLED[i] = CRGB::Green;
        } else {
            // Sync blink: brief red flash on a shared period so all scarves pulse together.
            float    floatTime = _timeMsec / (float)kSyncBlinkPeriodMs;
            uint32_t intTime   = _timeMsec / kSyncBlinkPeriodMs;
            constexpr float kDutyCycle = 0.05f;
            if ((floatTime - (float)intTime) < kDutyCycle)
                _builtinLED[i] = CRGB::Red;
        }
    }
}

void Scarf::showLEDs()
{
    _patternManager->runCurrentPattern(_ledsReal, _mesh->timeMs(), _tapTempo.beatInfo(_timeMsec));
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
