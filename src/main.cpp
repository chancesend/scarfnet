#include "main.h"
#include "PatternManager.h"
#include "defines.h"
#include "Scarf.h"
#include "Mesh.h"
#include "log.h"

#include <memory>

Scarfnet::Scarf scarf;

// This is pre-defined by arduino
void setup()
{
    Serial.begin(115200);

    Scheduler userScheduler;

    Scarfnet::MeshConnection connection;
    connection.ssid = kMeshSSID;
    connection.password = kMeshPassword;
    connection.port = kMeshPort;
    scarf.setup();
}

// This is pre-defined by arduino
void loop()
{
//    Scarfnet::log("loop()");

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
        return;
    }
}


uint32_t get_millisecond_timer()
{
    auto currentMsec = scarf.getTimeMsec();
    return currentMsec;
}