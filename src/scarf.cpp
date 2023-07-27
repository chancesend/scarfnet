#include "Scarf.h"

#include "defines.h"
#include "patterns.h"

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
    _patterns.push_back({"pride", [](led_list& leds, int32_t time) {
        pride(leds);
    }});
    _patterns.push_back({"confetti", [](led_list& leds, int32_t time) {
        confetti(leds);
    }});
    _patterns.push_back({"firework", [](led_list& leds, int32_t time) {
        firework(leds);
    }});

    _currentPattern = _patterns.begin();
}

void Scarf::sendMessage() {
    DynamicJsonDocument doc(1024);

    doc["id"] = _mesh->getNodeId();
    doc["lastPress"]   = _lastSelfButtonPress;
    doc["pattern"] = _currentPattern->first;

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
    _blinkNoNodes.enableDelayed(kBlinkPeriod - (_mesh->getNodeTime() % (kBlinkPeriod*1000))/1000);
}

void Scarf::onReceivedData(const DynamicJsonDocument& doc)
{
    const char* presetName = doc["pattern"];
    const int   lastRemoteButtonPress = doc["lastPress"];
    const int   nodeId = doc["id"];
    Serial.printf("Scarf::onReceivedData(). lastRemote = %d, _lastanyMesh = %d, _lastSelfButton = %d\n", 
    lastRemoteButtonPress, _lastAnyMeshPress, _lastSelfButtonPress);
    if (lastRemoteButtonPress > _lastAnyMeshPress) {
        _lastAnyMeshPress = lastRemoteButtonPress;
        if (_lastAnyMeshPress > _lastSelfButtonPress)
        {
            Serial.printf("Scarf::onReceivedData(). Changing pattern to %s\n", presetName);
            changePatternFromString(presetName);
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
        if (event == ObservableButton::Event::ePress) {
            _lastSelfButtonPress = this->_mesh->getNodeTime();
            incrementPattern();
            _taskSendMessage.forceNextIteration();
        }
    });

    _leds.resize(kNumLeds);
    _ledsReal.resize(kNumLeds);
    FastLED.addLeds<LED_TYPE, kLedPin>(_ledsReal.data(), _ledsReal.size());

    // tell FastLED there's 1 builtin led
    _builtinLED.resize(kNumBuiltinLeds);
    FastLED.addLeds<LED_TYPE, kBuiltinLedPin>(_builtinLED.data(), _builtinLED.size());

    set_max_power_in_volts_and_milliamps(5, 500);               // FastLED Power management set at 5V, 500mA.
  
    _userScheduler.addTask( _taskSendMessage );
    _taskSendMessage.enable();

    _userScheduler.addTask( _taskCurrentPatternRun );
    // TODO: enable this task?  
    // _taskCurrrentPatternRun.enable();

    _blinkNoNodes.set(kBlinkPeriod, _mesh->getNumNodes() * 2, 
        [&](){ this->blinkNumNodes(); });
    _userScheduler.addTask(_blinkNoNodes);
    _blinkNoNodes.enable();

//    _nextPatternButton.begin();

    randomSeed(micros());
}

void Scarf::loop()
{
    // put your main code here, to run repeatedly:

//  M5.update();
    _mesh->update();

    EVERY_N_MILLISECONDS(15) {
        updateTime();
        showLEDs();
        showBuiltInLED();
    }
    EVERY_N_MILLISECONDS(1000) {
        watchdog();
    }
}

void Scarf::watchdog()
{
    Serial.printf("Scarf::watchdogTasks()\n");
}

void Scarf::updateTime()
{
    _timeUsec = _mesh->getNodeTime();
}

void Scarf::showBuiltInLED() {
    auto color = !_onFlag ? CRGB::White : CRGB::Black;
    for (int i = 0; i < _builtinLED.size(); ++i)
    {
        _builtinLED[i] = !color;
    }
}

// Generate the animation every (ms)
void Scarf::showLEDs() {
    _currentPattern->second(_leds, _mesh->getNodeTime());

    for (int i = 0; i < _leds.size(); i++) {
        _ledsReal[i] = _leds[i];
    }
#if 1
    if ((_timeUsec / _usecPeriod) > _timeSec)
    {
        // We know that our mesh is at the start of our period,
        // so flash the number of LEDs to correspond to the number
        // of nodes we have
        int numLeds = 1;//_mesh->getNumNodes();
        for (int i = 0; i < numLeds; i++) {
            _ledsReal[i].setRGB(100, 100, 100);
        }
        _timeSec = (_timeUsec / _usecPeriod);
    }
#endif

    FastLED.show();
}


bool blinkState = false;

void Scarf::blinkNumNodes() {
    const int kBlinkDuration = 100;  // milliseconds LED is on for
    // If on, switch off, else switch on
    if (_onFlag)
        _onFlag = false;
    else
        _onFlag = true;
    _blinkNoNodes.delay(kBlinkDuration);

    if (_blinkNoNodes.isLastIteration()) {
        // Finished blinking. Reset task for next run 
        // blink number of nodes (including this node) times
        _blinkNoNodes.setIterations(_mesh->getNumNodes() * 2);
        // Calculate delay based on current mesh time and kBlinkPeriod
        // This results in blinks between nodes being synced
        _blinkNoNodes.enableDelayed(kBlinkPeriod - 
            (_mesh->getNodeTime() % (kBlinkPeriod*1000))/1000);
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
    Serial.printf("Scarf::incrementPattern(). Changing pattern to %s\n", newPattern->first);
    changePatternFromString(newPattern->first);
}

void Scarf::changePatternFromString(const std::string& pattern)
{
    auto found = std::find_if(_patterns.begin(), _patterns.end(), [pattern](const NamedPattern& it)->bool {
        return (pattern == it.first);
    });
    if (found != _patterns.end())
    {
        _currentPattern = found;
        Serial.printf("Changing pattern: %s\n", found->first.c_str());
    }
    else {
        Serial.printf("Pattern: %s not found!\n", found->first.c_str());
    }
}

}   // namespace Scarf
