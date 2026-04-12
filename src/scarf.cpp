#include "Scarf.h"

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
    _taskSendMessage(TASK_SECOND * 3, TASK_FOREVER,
        [this]()
        { sendMessage(); }),
    _taskLogMemory(TASK_MINUTE, TASK_FOREVER,
        [this]()
        {
            Scarfnet::log("[MEM] free: %u  min-free: %u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
        })
{
    Scarfnet::log("Scarf::Scarf()\n");
}

Rnd calcRandomizer(Mesh::TimeMs timeMs)
{
    return (Rnd)timeMs;
}

void Scarf::sendMessage()
{
    JsonDocument doc;
    {
//        std::lock_guard<std::mutex> lock(_mutex);
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
    Serial.printf("[SND] %s\n", outJson.c_str());
}

void Scarf::onConnectionChange()
{
    Serial.printf("Scarf::onConnectionChange()\n");
    _blinkNoNodes.setIterations(_mesh->getNumNodes() * 2);
    _blinkNoNodes.enableDelayed(kBlinkPeriodMs - (_mesh->getNodeTimeMs() % kBlinkPeriodMs) / 1000);
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

        Serial.printf("Scarf::onReceivedData(). Changing pattern to %s (randomizer %i)\n", presetName, randomizer);
        _patternManager->changePatternFromString(presetName, randomizer);
    }
}

void Scarf::setup()
{
    Serial.printf("Scarf::setup()\n");
    // put your setup code here, to run once:

    _preferences.begin("scarfNet", false); // Namespace for non-volatile parameters
    bool isLedTypeSet = _preferences.isKey(kLedTypeString);
    if (!isLedTypeSet) {
        _preferences.putInt(kLedTypeString, kLedType_Amazon);
        Scarfnet::log("LED type not set in preferences. Setting to default %i\n", kLedType_Amazon);
    } else {
        Scarfnet::log("LED type already set in preferences\n");
    }
    int ledType = _preferences.getInt(kLedTypeString, kLedType_Amazon);
    Scarfnet::log("Loading LED type %i from preferences(%s)\n", ledType, kLedTypeString);

#if 0
M5.begin(
true,  // SerialEnable
false, // I2CEnable
true   // DisplayEnable
);
#endif

    _mesh = make_unique<Mesh>(kMeshSSID, kMeshPassword, &_userScheduler, kMeshPort);
    _mesh->addConnectionObserver([&]()
                                    { 
        Scarfnet::log("### addConnectionObserver() callback\n");
                                        this->onConnectionChange(); });
    _mesh->addReceivedDataObserver([&](const JsonDocument &doc)
                                    { 
//        Scarfnet::log("### addReceivedDataObserver() callback\n");
        this->onReceivedData(doc); });

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

    // tell FastLED there's 1 builtin led
    _builtinLED.resize(kNumBuiltinLeds);
    FastLED.addLeds<M5_INTERNAL_TYPE, kBuiltinLedPin>(_builtinLED.data(), _builtinLED.size());

    FastLED.setMaxPowerInMilliWatts(500);

    _userScheduler.addTask(_taskSendMessage);
    _taskSendMessage.enable();

    _userScheduler.addTask(_taskLogMemory);
    _taskLogMemory.enable();

    _blinkNoNodes.set(kBlinkPeriodMs, _mesh->getNumNodes() * 2,
                        [&]()
                        { this->blinkNumNodes(); });
    _userScheduler.addTask(_blinkNoNodes);
    //_blinkNoNodes.enable();

    randomSeed(micros());
    Scarfnet::log("Scarf::setup() done\n");
}

void Scarf::processEvent(const ObservableButton::Event &event)
{
    switch(event)
    {
        case ObservableButton::Event::ePress:
        {
  //          std::lock_guard<std::mutex> lock(_mutex);
            _lastSelfButtonPressMs = this->_mesh->getNodeTimeMs();
            _patternManager->incrementPattern(_lastSelfButtonPressMs);
            _changeIndex += 1;
            _taskSendMessage.forceNextIteration();
            Scarfnet::log("Event.ePress to pattern %s with randomizer %i (changeIndex: %i)\n", 
                _patternManager->getCurrentPattern().c_str(), calcRandomizer(_lastSelfButtonPressMs), _changeIndex);
            break;
        }
        case ObservableButton::Event::eLongPress:
        {
  //          std::lock_guard<std::mutex> lock(_mutex);
            _lastSelfButtonPressMs = this->_mesh->getNodeTimeMs();
            _patternManager->samePatternDifferentRandomizer(_lastSelfButtonPressMs);
            _changeIndex += 1;
            _taskSendMessage.forceNextIteration();
            Scarfnet::log("Event.eLongPress to pattern %s with randomizer %i (changeIndex: %i)\n", 
                _patternManager->getCurrentPattern().c_str(), calcRandomizer(_lastSelfButtonPressMs), _changeIndex);
            break;
        }
        case ObservableButton::Event::eExtraLongPress:
        {
            // Switch to the next LEDType, and save it in memory
  //          std::lock_guard<std::mutex> lock(_mutex);
            auto ledType = _preferences.getInt(kLedTypeString, kLedType_Amazon);
            ledType = (ledType + 1) % kLedType_Count;
            _preferences.putInt(kLedTypeString, ledType);
            Scarfnet::log("Event.eExtraLongPress. Changing LED type to %i and restarting\n", ledType);
            delay(1000);
            ESP.restart();
            break;
        }
    }
}

void Scarf::loop()
{
    // put your main code here, to run repeatedly:

//    M5.update();
    _mesh->update();
    updateTime();

    const int kLedRefreshRateMs = 15;
    EVERY_N_MILLISECONDS(kLedRefreshRateMs)
    {
        showLEDs();
        showBuiltInLED();
        FastLED.show();
    }
    const int kPaletteBlendRate = 40;
    EVERY_N_MILLISECONDS(kPaletteBlendRate)
    {
        _patternManager->blendPalette();
    }
    const int kMeshCleanupIntervalMs = 60000;
    EVERY_N_MILLISECONDS(kMeshCleanupIntervalMs)    {
        _mesh->cleanupDisconnectedNodes();
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

        // We also do a sync blink every few seconds
        float floatTime = _timeMsec / (float)_syncBlinkPeriodMs;
        uint32_t intTime = _timeMsec / _syncBlinkPeriodMs;
        float kDutyCycle = 0.05f;
        if ((floatTime - (float)intTime) < kDutyCycle)
        {
            _builtinLED[i] = syncColor;
        }
    }
}

// Generate the animation every N ms
void Scarf::showLEDs()
{
    _patternManager->runCurrentPattern(_ledsReal, _mesh->getNodeTimeMs());
}

void Scarf::blinkNumNodes()
{
    const int kBlinkDurationMs = 100; // milliseconds LED is on for
    _onFlag = !_onFlag;
    _blinkNoNodes.delay(kBlinkDurationMs);

    if (_blinkNoNodes.isLastIteration())
    {
        // Finished blinking. Reset task for next run
        // blink number of nodes (including this node) times
        _blinkNoNodes.setIterations(_mesh->getNumNodes() * 2);
        // Calculate delay based on current mesh time and kBlinkPeriodMs
        // This results in blinks between nodes being synced
        auto msToNextBlinkSync = kBlinkPeriodMs -
            (_mesh->getNodeTimeMs() % kBlinkPeriodMs);
        _blinkNoNodes.enableDelayed(msToNextBlinkSync);
    }
}

} // namespace Scarfnet
