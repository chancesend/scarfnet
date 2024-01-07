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

    Serial.printf("setup() 1\n");
    Scheduler userScheduler;

    Scarfnet::MeshConnection connection;
    connection.ssid = kMeshSSID;
    connection.password = kMeshPassword;
    connection.port = kMeshPort;
    Serial.printf("setup() 2\n");
    scarf.setup();
}

// This is pre-defined by arduino
void loop()
{
//    Scarfnet::log("loop()\n");

    try
    {
        scarf.loop();
    }
    catch (std::exception& e)
    {
        Scarfnet::log("Exception: %s\n", e.what());
        return;
    }
}


uint32_t get_millisecond_timer()
{
    auto currentMsec = scarf.getTimeMsec();
    return currentMsec;
}