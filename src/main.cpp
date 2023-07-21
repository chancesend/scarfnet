//************************************************************
// this is a simple example that uses the easyMesh library
//
// 1. blinks led once for every node on the mesh
// 2. blink cycle repeats every kBlinkPeriod
// 3. sends a silly message to every node on the mesh at a random time between 1 and 5 seconds
// 4. prints anything it receives to Serial.print
//
//
//************************************************************
#include "main.h"
#include "patterns.h"
#include "defines.h"
#include "scarf.h"

Scarf scarf;

void setup() {
    scarf.setup();
}

void loop() {
    scarf.loop();
}