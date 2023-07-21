#include "scarf.h"

#include "defines.h"
#include "patterns.h"

#include <Arduino.h>

const int kBuiltinLedPin = 27; // GPIO number of builtin LED
const int kNumBuiltinLeds = 1;

const int kBlinkPeriod = 3000; // milliseconds until cycle repeat

const int kButtonPin = 39;
Button nextPatternButton(kButtonPin, 0, 10);

led_list leds;
led_list leds_real;

led_list builtin_led;


// Prototypes
void sendMessage(); 
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback(); 
void nodeTimeAdjustedCallback(int32_t offset); 
void delayReceivedCallback(uint32_t from, int32_t delay);


Scheduler     userScheduler; // to control your personal task
painlessMesh  mesh;

enum ButtonState
{
    kButtonState_Up,
    kButtonState_Pressed,
    kButtonState_LongPressed,
    kButtonState_DoublePressed
};

ButtonState buttonState = kButtonState_Up;
int32_t lastSelfButtonPress = 0;

bool calc_delay = false;
typedef SimpleList<uint32_t> NodeList;
NodeList nodes;

void checkButtonPress();
void sendMessage();
void currentPatternRun();
void blinkNumNodes();

void showBuiltInLED();

#define TASK_CHECK_BUTTON_PRESS_INTERVAL    50   // in milliseconds
#define CURRENTPATTERN_SELECT_DEFAULT_INTERVAL     1   // default scheduling time for currentPatternSELECT, in milliseconds

Task taskCheckButtonPress( TASK_CHECK_BUTTON_PRESS_INTERVAL, TASK_FOREVER, &checkButtonPress);
Task taskCurrentPatternRun( CURRENTPATTERN_SELECT_DEFAULT_INTERVAL, TASK_FOREVER, &currentPatternRun);
Task taskSendMessage( TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;

void init_mesh() {
    mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION | SYNC );  // set before init() so that you can see error messages
    mesh.init(kMeshSSID.c_str(), kMeshPassword.c_str(), &userScheduler, kMeshPort);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
    mesh.onNodeDelayReceived(&delayReceivedCallback);
}

Scarf::Scarf()
{
    
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

    init_mesh();

    leds.resize(kNumLeds);
    leds_real.resize(kNumLeds);
    FastLED.addLeds<LED_TYPE, kLedPin>(leds_real.data(), leds_real.size());

    // tell FastLED there's 1 builtin led
    builtin_led.resize(kNumBuiltinLeds);
    FastLED.addLeds<LED_TYPE, kBuiltinLedPin>(builtin_led.data(), builtin_led.size());

    set_max_power_in_volts_and_milliamps(5, 500);               // FastLED Power management set at 5V, 500mA.
  
    userScheduler.addTask( taskSendMessage );
    userScheduler.addTask( taskCheckButtonPress );
    userScheduler.addTask( taskCurrentPatternRun );
    taskSendMessage.enable();
    taskCheckButtonPress.enable() ;

    blinkNoNodes.set(kBlinkPeriod, getNumNodes() * 2, &blinkNumNodes);
    userScheduler.addTask(blinkNoNodes);
    blinkNoNodes.enable();

//    nextPatternButton.begin();

    randomSeed(micros());
}

void Scarf::loop()
{
    // put your main code here, to run repeatedly:
//  M5.update();
    mesh.update();

    EVERY_N_MILLISECONDS(15) {
        updateTime();
        showLEDs();
        showBuiltInLED();
    }
}

void Scarf::updateTime()
{
    _timeUsec = mesh.getNodeTime();
}

void Scarf::showBuiltInLED() {
    auto color = !onFlag ? CRGB::White : CRGB::Black;
    for (int i = 0; i < builtin_led.size(); ++i)
    {
        builtin_led[i] = !color;
    }
}

// Generate the animation every (ms)
void Scarf::showLEDs() {
    int timeOnAnimation = 20.0;
    pride(leds);
//    fillNoise(mesh.getNodeTime());

    for (int i = 0; i < leds.size(); i++) {
        leds_real[i] = leds[i];
    }

    if ((_timeUsec / _usecPeriod) > _timeSec)
    {
        // We know that our mesh is at the start of our period,
        // so flash the number of LEDs to correspond to the number
        // of nodes we have
        int numLeds = 1;//getNumNodes();
        for (int i = 0; i < numLeds; i++) {
            leds_real[i].setRGB(100, 100, 100);
        }
        _timeSec = (_timeUsec / _usecPeriod);
    }

    FastLED.show();
}


bool blinkState = false;

void delay_calc(const NodeList& nodes) {
    if (calc_delay) {
        for (NodeList::const_iterator node = nodes.begin(); node != nodes.end(); ++node) {
            mesh.startDelayMeas(*node);
        }
        calc_delay = false;
    }
}

void sendMessage() {
    String msg = "ID: ";
    msg += mesh.getNodeId();
    msg += ", last_press = ";
    msg += lastSelfButtonPress;
//  msg += " myFreeMemory: " + String(ESP.getFreeHeap());
    mesh.sendBroadcast(msg);

    delay_calc(nodes);

    Serial.printf("Sending message: %s\n", msg.c_str());
    
    taskSendMessage.setInterval( random(TASK_SECOND * 1, TASK_SECOND * 5));  // between 1 and 5 seconds
}

void blinkNumNodes() {
    const int kBlinkDuration = 100;  // milliseconds LED is on for
    // If on, switch off, else switch on
    if (onFlag)
        onFlag = false;
    else
        onFlag = true;
    blinkNoNodes.delay(kBlinkDuration);

    if (blinkNoNodes.isLastIteration()) {
        // Finished blinking. Reset task for next run 
        // blink number of nodes (including this node) times
        blinkNoNodes.setIterations(getNumNodes() * 2);
        // Calculate delay based on current mesh time and kBlinkPeriod
        // This results in blinks between nodes being synced
        blinkNoNodes.enableDelayed(kBlinkPeriod - 
            (mesh.getNodeTime() % (kBlinkPeriod*1000))/1000);
    }
}

int getNumNodes()
{
    return (mesh.getNodeList().size() + 1);
}


void receivedCallback(uint32_t from, String & msg) {
    Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    // Reset blink task
    onFlag = false;
    blinkNoNodes.setIterations(getNumNodes() * 2);
    blinkNoNodes.enableDelayed(kBlinkPeriod - (mesh.getNodeTime() % (kBlinkPeriod*1000))/1000);
    
    Serial.printf("--> startHere: New Connection, nodeId = %u, %s\n", nodeId, mesh.subConnectionJson(true).c_str());
}

void print_connection_list(const NodeList& nodes) {
    Serial.printf("Connection list (%d nodes):", nodes.size());

    for (NodeList::const_iterator node = nodes.begin(); node != nodes.end(); node++) {
        Serial.printf(" %u", *node);
    }
    Serial.println();
}

void changedConnectionCallback() {
    Serial.printf("Changed connections\n");
    // Reset blink task
    onFlag = false;
    blinkNoNodes.setIterations(getNumNodes() * 2);
    blinkNoNodes.enableDelayed(kBlinkPeriod - (mesh.getNodeTime() % (kBlinkPeriod*1000))/1000);
    
    NodeList nodes = mesh.getNodeList();

    print_connection_list(nodes);
    calc_delay = true;
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void delayReceivedCallback(uint32_t from, int32_t delay) {
    Serial.printf("Delay to node %u is %d us\n", from, delay);
}

void checkButtonPress()
{
    const int32_t kLongPressTimeMs = 1000;
    nextPatternButton.read();
    if (nextPatternButton.pressedFor(kLongPressTimeMs) && nextPatternButton.isPressed() && buttonState != kButtonState_LongPressed)
    {
        buttonState = kButtonState_LongPressed;
        Serial.printf("Long press!\n");
    }
    else if( nextPatternButton.wasPressed() ) {
        buttonState = kButtonState_Pressed;
        Serial.printf("Button press!\n");
        //selectNextPattern();
        lastSelfButtonPress = mesh.getNodeTime();
    }
    else if (nextPatternButton.wasReleased())
    {
        buttonState = kButtonState_Up;
    }
}

void currentPatternRun()
{

}
