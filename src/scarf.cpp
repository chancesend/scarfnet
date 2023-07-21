#include "Scarf.h"

#include "defines.h"
#include "patterns.h"

#include <Arduino.h>

const int kBuiltinLedPin = 27; // GPIO number of builtin LED

#define TASK_CHECK_BUTTON_PRESS_INTERVAL    50   // in milliseconds
#define CURRENTPATTERN_SELECT_DEFAULT_INTERVAL     1   // default scheduling time for currentPatternSELECT, in milliseconds


Scarf::Scarf() :
    _nextPatternButton(kButtonPin, 0, 10),
    _mesh(kMeshSSID, kMeshPassword, &_userScheduler, kMeshPort),
    _taskCheckButtonPress( TASK_CHECK_BUTTON_PRESS_INTERVAL, TASK_FOREVER, 
        [&](){ _button->checkButtonPress(); }),
    _taskCurrentPatternRun( CURRENTPATTERN_SELECT_DEFAULT_INTERVAL, TASK_FOREVER, 
        [&](){ this->currentPatternRun(); }),
    _taskSendMessage( TASK_SECOND * 1, TASK_FOREVER, 
        [&](){ this->sendMessage(); }) // start with a one second interval
{
    _mesh.addConnectionObserver(this);
}

void Scarf::sendMessage() {
    String msg = "ID: ";
    msg += _mesh.getNodeId();
    msg += ", last_press = ";
    msg += lastSelfButtonPress;
//  msg += " myFreeMemory: " + String(ESP.getFreeHeap());
    _mesh.sendBroadcast(msg);

    _mesh.doDelayCalc();

    Serial.printf("Sending message: %s\n", msg.c_str());
    
    _taskSendMessage.setInterval( random(TASK_SECOND * 1, TASK_SECOND * 5));  // between 1 and 5 seconds
}

void Scarf::onConnectionChange()
{
    _blinkNoNodes.setIterations(_mesh.getNumNodes() * 2);
    _blinkNoNodes.enableDelayed(kBlinkPeriod - (_mesh.getNodeTime() % (kBlinkPeriod*1000))/1000);
}

void Scarf::setup()
{
    // put your setup code here, to run once:
    #if 0
    M5.begin(
        true,  // SerialEnable
        false, // I2CEnable
        true   // DisplayEnable
    );
    #endif

    Serial.begin(115200);
    Serial.printf("Hello world!");

    _leds.resize(kNumLeds);
    _ledsReal.resize(kNumLeds);
    FastLED.addLeds<LED_TYPE, kLedPin>(_ledsReal.data(), _ledsReal.size());

    // tell FastLED there's 1 builtin led
    _builtinLED.resize(kNumBuiltinLeds);
    FastLED.addLeds<LED_TYPE, kBuiltinLedPin>(_builtinLED.data(), _builtinLED.size());

    set_max_power_in_volts_and_milliamps(5, 500);               // FastLED Power management set at 5V, 500mA.
  
    _userScheduler.addTask( _taskSendMessage );
    _userScheduler.addTask( _taskCheckButtonPress );
    _userScheduler.addTask( _taskCurrentPatternRun );
    _taskSendMessage.enable();
    _taskCheckButtonPress.enable() ;

    _blinkNoNodes.set(kBlinkPeriod, _mesh.getNumNodes() * 2, 
        [&](){ this->blinkNumNodes(); });
    _userScheduler.addTask(_blinkNoNodes);
    _blinkNoNodes.enable();

//    nextPatternButton.begin();

    randomSeed(micros());
}

void Scarf::loop()
{
    // put your main code here, to run repeatedly:
//  M5.update();
    _mesh.update();

    EVERY_N_MILLISECONDS(15) {
        updateTime();
        showLEDs();
        showBuiltInLED();
    }
}

void Scarf::updateTime()
{
    _timeUsec = _mesh.getNodeTime();
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
    int timeOnAnimation = 20.0;
    pride(_leds);
//    fillNoise(mesh.getNodeTime());

    for (int i = 0; i < _leds.size(); i++) {
        _ledsReal[i] = _leds[i];
    }

    if ((_timeUsec / _usecPeriod) > _timeSec)
    {
        // We know that our mesh is at the start of our period,
        // so flash the number of LEDs to correspond to the number
        // of nodes we have
        int numLeds = _mesh.getNumNodes();
        for (int i = 0; i < numLeds; i++) {
            _ledsReal[i].setRGB(100, 100, 100);
        }
        _timeSec = (_timeUsec / _usecPeriod);
    }

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
        _blinkNoNodes.setIterations(_mesh.getNumNodes() * 2);
        // Calculate delay based on current mesh time and kBlinkPeriod
        // This results in blinks between nodes being synced
        _blinkNoNodes.enableDelayed(kBlinkPeriod - 
            (_mesh.getNodeTime() % (kBlinkPeriod*1000))/1000);
    }
}

void Scarf::checkButtonPress()
{
    const int32_t kLongPressTimeMs = 1000;
    _nextPatternButton.read();
    if (_nextPatternButton.pressedFor(kLongPressTimeMs) && _nextPatternButton.isPressed() && buttonState != kButtonState_LongPressed)
    {
        buttonState = kButtonState_LongPressed;
        Serial.printf("Long press!\n");
    }
    else if( _nextPatternButton.wasPressed() ) {
        buttonState = kButtonState_Pressed;
        Serial.printf("Button press!\n");
        //selectNextPattern();
        lastSelfButtonPress = _mesh.getNodeTime();
    }
    else if (_nextPatternButton.wasReleased())
    {
        buttonState = kButtonState_Up;
    }
}

void Scarf::currentPatternRun()
{

}
