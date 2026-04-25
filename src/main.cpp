#include "main.h"
#include "PatternManager.h"
#include "defines.h"
#include "scarf.h"
#include "log.h"

#include <memory>

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

uint32_t get_millisecond_timer()
{
    return scarf.getTimeMsec();
}
