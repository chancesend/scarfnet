#include "Scarf.h"

#include "config.h"
#include "defines.h"
#include "patterns.h"
#include "palettes.h"
#include "log.h"
//#include <Arduino.h>
#include <ArduinoJson.h>

namespace Scarfnet
{

static const char* const kLedTypeString = "ledType";

Scarf::Scarf() :
    _patternManager(std::make_shared<Scarfnet::PatternManager>()),
    _nextPatternButton(&_userScheduler, kButtonPin),
    _taskSendMessage(kHeartbeatIntervalMs, TASK_FOREVER,
        [this]()
        { sendMessage(); }),
    _taskLogMemory(kMemLogIntervalMs, TASK_FOREVER,
        [this]()
        {
            Scarfnet::log("[MEM] free: %u  min-free: %u",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
        })
{
    Scarfnet::log("Scarf::Scarf()");
}

Rnd calcRandomizer(Mesh::TimeMs timeMs)
{
    return (Rnd)timeMs;
}

void Scarf::sendMessage()
{
    JsonDocument doc;
    {
        doc["id"] = _mesh->getNodeId();
        doc["lastPress"] = _lastSelfButtonPressMs;
        doc["pattern"] = _patternManager->getCurrentPattern();
        doc["randomizer"] = calcRandomizer(_lastSelfButtonPressMs);
        doc["currentTimeMs"] = _mesh->getNodeTimeMs();
        doc["changeIndex"] = _changeIndex;
    }

    String outJson;
    serializeJson(doc, outJson);
    _mesh->sendBroadcast(outJson);

    _mesh->doDelayCalc();
    Scarfnet::log("[SND] %s", outJson.c_str());
}

void Scarf::onConnectionChange()
{
    Scarfnet::log("Scarf::onConnectionChange()");
    _blinkNoNodes.setIterations(_mesh->getNumNodes() * 2);
    _blinkNoNodes.enableDelayed(kNodeBlinkPeriodMs - (_mesh->getNodeTimeMs() % kNodeBlinkPeriodMs) / 1000);
    _mesh->doDelayCalc();
}

void Scarf::onReceivedData(const JsonDocument &doc)
{
    const char *presetName = doc["pattern"];
    const Mesh::TimeMs lastRemoteButtonPressMs = doc["lastPress"];
    const Rnd randomizer = doc["randomizer"];
    const uint32_t changeIndex = doc["changeIndex"];
    if (changeIndex > _changeIndex)
    {
        // changeIndex and lastPress are sensitive to rollover
        // (though not for several days or millions of presses).
        // Cap at 0x7fffffff to force-roll before overflow locks us out.
        _changeIndex = changeIndex > 0x7fffffff ? 0 : changeIndex;
        _lastSelfButtonPressMs = lastRemoteButtonPressMs > 0x7fffffff ? 0 : lastRemoteButtonPressMs;

        Scarfnet::log("Scarf::onReceivedData(). Changing pattern to %s (randomizer %i)", presetName, randomizer);
        _patternManager->changePatternFromString(presetName, randomizer);
    }
}

void Scarf::setup()
{
    Scarfnet::log("Scarf::setup()");

    // Init builtin LED early so OtaManager can use it for visual feedback.
    _builtinLED.resize(kNumBuiltinLeds);
    FastLED.addLeds<M5_INTERNAL_TYPE, kBuiltinLedPin>(_builtinLED.data(), _builtinLED.size());

    _otaManager = make_unique<OtaManager>(kButtonPin, [this](CRGB color)
    {
        for (auto& led : _builtinLED) { led = color; }
        FastLED.show();
    });
    if (_otaManager->checkBootTrigger())
        return; // skip normal setup entirely

    _preferences.begin("scarfNet", false); // Namespace for non-volatile parameters
    bool isLedTypeSet = _preferences.isKey(kLedTypeString);
    if (!isLedTypeSet) {
        _preferences.putInt(kLedTypeString, kLedType_Amazon);
        Scarfnet::log("LED type not set in preferences. Setting to default %i", kLedType_Amazon);
    } else {
        Scarfnet::log("LED type already set in preferences");
    }
    int ledType = _preferences.getInt(kLedTypeString, kLedType_Amazon);
    Scarfnet::log("Loading LED type %i from preferences(%s)", ledType, kLedTypeString);

    _mesh = make_unique<Mesh>(kMeshSSID, kMeshPassword, &_userScheduler, kMeshPort);
    _mesh->addConnectionObserver([&]()
                                    {
        Scarfnet::log("[MESH] addConnectionObserver() callback");
                                        this->onConnectionChange(); });
    _mesh->addReceivedDataObserver([&](const JsonDocument &doc)
                                    { this->onReceivedData(doc); });

    _nextPatternButton.setup();
    _nextPatternButton.addObserver([&](const ObservableButton::Event &event)
                                    { this->processEvent(event); });

    _ledsReal.resize(kNumLeds);
    switch (ledType) {
        case kLedType_Adafruit:
             FastLED.addLeds<ADAFRUIT, kLedPin>(_ledsReal.data(), _ledsReal.size());
             break;
        case kLedType_Amazon:
             FastLED.addLeds<AMAZON, kLedPin>(_ledsReal.data(), _ledsReal.size());
            break;
    };

    FastLED.setMaxPowerInMilliWatts(500);

    _userScheduler.addTask(_taskSendMessage);
    _taskSendMessage.enable();

    _userScheduler.addTask(_taskLogMemory);
    _taskLogMemory.enable();

    _blinkNoNodes.set(kNodeBlinkPeriodMs, _mesh->getNumNodes() * 2,
                        [&]()
                        { this->blinkNumNodes(); });
    _userScheduler.addTask(_blinkNoNodes);
    //_blinkNoNodes.enable();

    randomSeed(micros());
    Scarfnet::log("Scarf::setup() done");
}

void Scarf::processEvent(const ObservableButton::Event &event)
{
    switch(event)
    {
        case ObservableButton::Event::ePress:
        {
            _lastSelfButtonPressMs = this->_mesh->getNodeTimeMs();
            _patternManager->incrementPattern(_lastSelfButtonPressMs);
            _changeIndex += 1;
            _taskSendMessage.forceNextIteration();
            Scarfnet::log("Event.ePress to pattern %s with randomizer %i (changeIndex: %i)",
                _patternManager->getCurrentPattern().c_str(), calcRandomizer(_lastSelfButtonPressMs), _changeIndex);
            break;
        }
        case ObservableButton::Event::eLongPress:
        {
            _lastSelfButtonPressMs = this->_mesh->getNodeTimeMs();
            _patternManager->samePatternDifferentRandomizer(_lastSelfButtonPressMs);
            _changeIndex += 1;
            _taskSendMessage.forceNextIteration();
            Scarfnet::log("Event.eLongPress to pattern %s with randomizer %i (changeIndex: %i)",
                _patternManager->getCurrentPattern().c_str(), calcRandomizer(_lastSelfButtonPressMs), _changeIndex);
            break;
        }
        case ObservableButton::Event::eExtraLongPress:
        {
            auto ledType = _preferences.getInt(kLedTypeString, kLedType_Amazon);
            ledType = (ledType + 1) % kLedType_Count;
            _preferences.putInt(kLedTypeString, ledType);
            Scarfnet::log("Event.eExtraLongPress. Changing LED type to %i and restarting", ledType);
            delay(1000);
            ESP.restart();
            break;
        }
    }
}

void Scarf::loop()
{
    _mesh->update();
    updateTime();

    EVERY_N_MILLISECONDS(kLedRefreshRateMs)
    {
        showLEDs();
        showBuiltInLED();
        FastLED.show();
    }
    EVERY_N_MILLISECONDS(kPaletteBlendRateMs)
    {
        _patternManager->blendPalette();
    }
}

void Scarf::updateTime()
{
    _timeMsec = _mesh->getNodeTimeMs();
}

void Scarf::showBuiltInLED()
{
    auto syncColor = CRGB::Red;
    for (int i = 0; i < _builtinLED.size(); ++i)
    {
        _builtinLED[i] = CRGB::Black;

        // Sync blink: brief red flash on a shared period so all scarves pulse together
        float floatTime = _timeMsec / (float)kSyncBlinkPeriodMs;
        uint32_t intTime = _timeMsec / kSyncBlinkPeriodMs;
        const float kDutyCycle = 0.05f;
        if ((floatTime - (float)intTime) < kDutyCycle)
        {
            _builtinLED[i] = syncColor;
        }
    }
}

void Scarf::showLEDs()
{
    _patternManager->runCurrentPattern(_ledsReal, _mesh->getNodeTimeMs());
}

void Scarf::blinkNumNodes()
{
    const int kBlinkDurationMs = 100;
    _onFlag = !_onFlag;
    _blinkNoNodes.delay(kBlinkDurationMs);

    if (_blinkNoNodes.isLastIteration())
    {
        _blinkNoNodes.setIterations(_mesh->getNumNodes() * 2);
        // Sync blink start time across nodes using mesh time
        auto msToNextBlinkSync = kNodeBlinkPeriodMs -
            (_mesh->getNodeTimeMs() % kNodeBlinkPeriodMs);
        _blinkNoNodes.enableDelayed(msToNextBlinkSync);
    }
}

void Scarf::otaLoop()
{
    _otaManager->loop();
}

} // namespace Scarfnet
