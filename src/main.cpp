#include "main.h"
#include "patterns.h"
#include "defines.h"
#include "Scarf.h"

Scarf::Scarf scarf;

// This is pre-defined by arduino
void setup() {
    Serial.printf("setup()\n");
    scarf.setup();
}

// This is pre-defined by arduino
void loop() {
    scarf.loop();
}