#include "scarf.h"
#include "log.h"

Scarfnet::Scarf scarf;

void setup()
{
    Serial.begin(115200);
    scarf.setup();
}

void loop()
{
    try
    {
        if (scarf.isInOtaMode())
            scarf.otaLoop();
        else
            scarf.loop();
    }
    catch (std::exception& e)
    {
        Scarfnet::log("Exception: %s", e.what());
    }
}
