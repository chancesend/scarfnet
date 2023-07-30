#include "Scarf.h"

#include "defines.h"
#include "patterns.h"
#include "palettes.h"

#include <Arduino.h>
#include <ArduinoJson.h>

namespace Scarf
{

const int kBuiltinLedPin = 27; // GPIO number of builtin LED

#define CURRENTPATTERN_SELECT_DEFAULT_INTERVAL     1   // default scheduling time for currentPatternSELECT, in milliseconds

Scarf::Scarf() :
    _nextPatternButton(&_userScheduler, kButtonPin),
    _taskCurrentPatternRun( CURRENTPATTERN_SELECT_DEFAULT_INTERVAL, TASK_FOREVER, 
        [&](){ this->currentPatternRun(); }),
    _taskSendMessage( TASK_SECOND * 3, TASK_FOREVER, 
        [&](){ this->sendMessage(); }) // start with a one second interval
{
    Serial.printf("Scarf::Scarf()\n");

    initPatterns();
}

void Scarf::initPatterns()
{
    getPatternList(_patterns);
    _currentPattern = _patterns.begin();
}

void Scarf::sendMessage() {
    DynamicJsonDocument doc(1024);

    doc["id"] = _mesh->getNodeId();
    doc["lastPress"]   = _lastSelfButtonPressMs;
    doc["pattern"] = _currentPattern->first;
    doc["randomizer"] = (Rnd)_lastSelfButtonPressMs & (~((Rnd)0));

    String outJson;
    serializeJson(doc, outJson);
    _mesh->sendBroadcast(outJson);

    _mesh->doDelayCalc();
    Serial.printf("Sending message: %s\n", outJson.c_str());
}

void Scarf::onConnectionChange()
{
    Serial.printf("Scarf::onConnectionChange()\n");
    _blinkNoNodes.setIterations(_mesh->getNumNodes() * 2);
    _blinkNoNodes.enableDelayed(kBlinkPeriodMs - (_mesh->getNodeTimeMs() % kBlinkPeriodMs)/1000);
}

void Scarf::onReceivedData(const DynamicJsonDocument& doc)
{
    const char* presetName = doc["pattern"];
    const Mesh::TimeMs   lastRemoteButtonPressMs = doc["lastPress"];
    const int   nodeId = doc["id"];
    const Rnd randomizer = doc["randomizer"];
    const bool isNewRemotePress = (lastRemoteButtonPressMs > _lastAnyMeshPressMs);
    if (isNewRemotePress) {
        _lastAnyMeshPressMs = lastRemoteButtonPressMs;
        const bool isRemoteButtonWasLastPressed = (_lastAnyMeshPressMs > _lastSelfButtonPressMs);
        if (isRemoteButtonWasLastPressed)
        {
            Serial.printf("Scarf::onReceivedData(). Changing pattern to %s (randomizer %i)\n", presetName, randomizer);
            changePatternFromString(presetName, randomizer);
        }
    }
}

void Scarf::setup()
{
    Serial.printf("Scarf::setup()\n");
    // put your setup code here, to run once:

    #if 0
    M5.begin(
        true,  // SerialEnable
        false, // I2CEnable
        true   // DisplayEnable
    );
    #endif

    Serial.begin(115200);
    Serial.printf("Hello world!\n");
    
    _mesh = make_unique<Mesh>(kMeshSSID, kMeshPassword, &_userScheduler, kMeshPort);
    _mesh->addConnectionObserver([&](){
        this->onConnectionChange();
    });
    _mesh->addReceivedDataObserver([&](const DynamicJsonDocument& doc){
        this->onReceivedData(doc);
    });

    _nextPatternButton.setup();
    _nextPatternButton.addObserver([&](const ObservableButton::Event& event){
        switch(event)
        {
            case ObservableButton::Event::ePress:
            {
                _lastSelfButtonPressMs = this->_mesh->getNodeTimeMs();
                incrementPattern();
                _taskSendMessage.forceNextIteration();
                break;
            }
            case ObservableButton::Event::eLongPress:
            {
                
                break;
            }
        }
    });

    _leds.resize(kNumLeds);
    _ledsReal.resize(kNumLeds);
    FastLED.addLeds<LED_TYPE, kLedPin>(_ledsReal.data(), _ledsReal.size());

    // tell FastLED there's 1 builtin led
    _builtinLED.resize(kNumBuiltinLeds);
    FastLED.addLeds<M5_INTERNAL_TYPE, kBuiltinLedPin>(_builtinLED.data(), _builtinLED.size());

    FastLED.setMaxPowerInVoltsAndMilliamps(5, 250); // FastLED Power management set at 5V, 200mA.

    _userScheduler.addTask( _taskSendMessage );
    _taskSendMessage.enable();

    _userScheduler.addTask( _taskCurrentPatternRun );
    // TODO: enable this task?  
    // _taskCurrrentPatternRun.enable();

    _blinkNoNodes.set(kBlinkPeriodMs, _mesh->getNumNodes() * 2, 
        [&](){ this->blinkNumNodes(); });
    _userScheduler.addTask(_blinkNoNodes);
    _blinkNoNodes.enable();

    randomSeed(micros());
}

void Scarf::loop()
{
    // put your main code here, to run repeatedly:

//  M5.update();
    _mesh->update();

    const int kLedRefreshRateMs = 15;
    EVERY_N_MILLISECONDS(kLedRefreshRateMs) {
        updateTime();
        showLEDs();
        showBuiltInLED();
        FastLED.show();
    }
    const int kWatchdogMs = 5000;
    EVERY_N_MILLISECONDS(kWatchdogMs) {
        watchdog();
    }
    EVERY_N_MILLISECONDS(40) {    
        nblendPaletteTowardPalette( _currentPalette, _targetPalette, 24);
    }
}

void Scarf::watchdog()
{
    Serial.printf("Scarf::watchdogTasks()\n");
}

void Scarf::updateTime()
{
    _timeMsec = _mesh->getNodeTimeMs();
}

void Scarf::showBuiltInLED() {
    auto color = _onFlag ? CRGB::Blue : CRGB::Black;
    auto syncColor = CRGB::Red;
    for (int i = 0; i < _builtinLED.size(); ++i)
    {
        // Every few second or we blink how many nodes are connected
        _builtinLED[i] = color;
        
        // We also do a sync blink every ~2 seconds
        uint32_t thisTimeMsec = _timeMsec / _msecPeriod;
        if (thisTimeMsec > _timeSec)
        {
            _builtinLED[i] += syncColor;
            _timeSec = thisTimeMsec;
        }
    }
    
}

// Generate the animation every (ms)
void Scarf::showLEDs() {
    _currentPattern->second(
        _leds, 
        _mesh->getNodeTimeMs(), 
        _currentPalette,
        _currentRandomizer);

    for (int i = 0; i < _leds.size(); i++) {
        _ledsReal[i] = _leds[i];
    }
}


bool blinkState = false;

void Scarf::blinkNumNodes() {
    const int kBlinkDurationMs = 100;  // milliseconds LED is on for
    _onFlag = !_onFlag;
    _blinkNoNodes.delay(kBlinkDurationMs);

    if (_blinkNoNodes.isLastIteration()) {
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

void Scarf::currentPatternRun()
{

}

void Scarf::incrementPattern()
{
    auto newPattern = _currentPattern + 1;
    if (newPattern == _patterns.end())
    {
        newPattern = _patterns.begin();
    }
    const auto newName = newPattern->first.c_str();
    Serial.printf("Scarf::incrementPattern(). Changing pattern to %s\n", newName);
    const auto randomizer = _lastSelfButtonPressMs;
    changePatternFromString(newPattern->first, randomizer);
}

void Scarf::changePatternFromString(const std::string& pattern, Rnd randomizer)
{
    auto found = std::find_if(_patterns.begin(), _patterns.end(), [pattern](const NamedPattern& it)->bool {
        return (pattern == it.first);
    });
    if (found != _patterns.end())
    {
        _currentPattern = found;
        _currentRandomizer = randomizer;
        _targetPalette = getColorPalette(randomizer);
        Serial.printf("Changing pattern: %s (randomizer %i)\n", found->first.c_str(), _currentRandomizer);
    }
    else {
        Serial.printf("Pattern: %s not found!\n", found->first.c_str());
    }
}

}   // namespace Scarf
