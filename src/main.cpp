//************************************************************
// this is a simple example that uses the easyMesh library
//
// 1. blinks led once for every node on the mesh
// 2. blink cycle repeats every BLINK_PERIOD
// 3. sends a silly message to every node on the mesh at a random time between 1 and 5 seconds
// 4. prints anything it receives to Serial.print
//
//
//************************************************************
#include "main.h"

// some gpio pin that is connected to an LED...
// on my rig, this is 5, change to the right number of your LED.
#define   LED             27       // GPIO number of connected LED, ON ESP-12 IS GPIO2

#define   BLINK_PERIOD    3000 // milliseconds until cycle repeat
#define   BLINK_DURATION  100  // milliseconds LED is on for

#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

#define LED_PIN     27 // This pin is ignorred when using FASTLED_ESP8266_DMA
#define NUM_LEDS    25
#define BRIGHTNESS  100 // Range 0 - 255
#define LED_TYPE    NEOPIXEL
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];
CRGB leds_real[NUM_LEDS];

static int16_t dist;  // A random number for our noise generator.
uint16_t xscale = 30;  // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint16_t yscale = 30;  // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint8_t maxChanges = 24;  // Value for blending between palettes.

CRGBPalette16 currentPalette(CRGB::White);

// Prototypes
void sendMessage(); 
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback(); 
void nodeTimeAdjustedCallback(int32_t offset); 
void delayReceivedCallback(uint32_t from, int32_t delay);

Scheduler     userScheduler; // to control your personal task
painlessMesh  mesh;

bool calc_delay = false;
SimpleList<uint32_t> nodes;

void sendMessage() ; // Prototype
Task taskSendMessage( TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;

void setup() {
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

    pinMode(LED, OUTPUT);

    mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION | SYNC );  // set before init() so that you can see error messages

    mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
    mesh.onNodeDelayReceived(&delayReceivedCallback);

    FastLED.addLeds<LED_TYPE, LED_PIN>(leds_real, NUM_LEDS);

//    set_max_power_in_volts_and_milliamps(5, 500);               // FastLED Power management set at 5V, 500mA.
  
    userScheduler.addTask( taskSendMessage );
    taskSendMessage.enable();

    blinkNoNodes.set(BLINK_PERIOD, (mesh.getNodeList().size() + 1) * 2, []() {
        // If on, switch off, else switch on
        if (onFlag)
            onFlag = false;
        else
            onFlag = true;
        blinkNoNodes.delay(BLINK_DURATION);

        if (blinkNoNodes.isLastIteration()) {
            // Finished blinking. Reset task for next run 
            // blink number of nodes (including this node) times
            blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
            // Calculate delay based on current mesh time and BLINK_PERIOD
            // This results in blinks between nodes being synced
            blinkNoNodes.enableDelayed(BLINK_PERIOD - 
                (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
        }
    });
    userScheduler.addTask(blinkNoNodes);
    blinkNoNodes.enable();

    randomSeed(micros());
}

bool blinkState = false;

void loop() {
    // put your main code here, to run repeatedly:
//  M5.update();
    mesh.update();
    digitalWrite(LED, !onFlag);

    EVERY_N_MILLISECONDS(1000) {
        leds_real[0] = blinkState ? CRGB::Red : CRGB::Black;
        leds_real[1] = blinkState ? CRGB::Red : CRGB::Black;
        leds_real[2] = blinkState ? CRGB::Red : CRGB::Black;
        blinkState = !blinkState;
        Serial.printf("LED state changed");
    }
}

void fillnoise8() {
  // Just ONE loop to fill up the LED array as all of the pixels change.
  for(int i = 0; i < NUM_LEDS; i++) {
    // Get a value from the noise function. I'm using both x and y axis.
    uint8_t index = inoise8(0, dist+i* yscale) % 255;
    // With that value, look up the 8 bit colour palette value and assign it to the current LED.
    leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);
  }
    dist += beatsin8(10,1,4, mesh.getNodeTime());                        
}

void fillNoise() {
  fillnoise8();
}

// Generate the animation every (ms)
void showLEDs() {
    int timeOnAnimation = 20.0;
    fillNoise();

    for (int i = 0; i < NUM_LEDS; i++) {
        leds_real[i] = leds[i];
    }

    FastLED.show();
}

void sendMessage() {
  String msg = "Hello from node ";
  msg += mesh.getNodeId();
  msg += " myFreeMemory: " + String(ESP.getFreeHeap());
  mesh.sendBroadcast(msg);

  if (calc_delay) {
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
      mesh.startDelayMeas(*node);
      node++;
    }
    calc_delay = false;
  }

  Serial.printf("Sending message: %s\n", msg.c_str());
  
  taskSendMessage.setInterval( random(TASK_SECOND * 1, TASK_SECOND * 5));  // between 1 and 5 seconds
}


void receivedCallback(uint32_t from, String & msg) {
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
 
  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
  Serial.printf("--> startHere: New Connection, %s\n", mesh.subConnectionJson(true).c_str());
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
 
  nodes = mesh.getNodeList();

  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) {
    Serial.printf(" %u", *node);
    node++;
  }
  Serial.println();
  calc_delay = true;
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void delayReceivedCallback(uint32_t from, int32_t delay) {
  Serial.printf("Delay to node %u is %d us\n", from, delay);
}