#include "Mesh.h"

namespace Scarfnet
{

Mesh::Mesh(std::string ssid, std::string password, Scheduler *scheduler, uint16_t port)
{
    _mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION | SYNC); // set before init() so that you can see error messages
    _mesh.init(ssid.c_str(), password.c_str(), scheduler, port);
    _mesh.onReceive([&](uint32_t from, TSTRING &msg)
                    { this->receivedCallback(from, msg); });
    _mesh.onNewConnection([&](uint32_t nodeId)
                            { this->newConnectionCallback(nodeId); });
    _mesh.onChangedConnections([&]()
                                { this->changedConnectionCallback(); });
    _mesh.onNodeTimeAdjusted([&](int32_t offset)
                                { this->nodeTimeAdjustedCallback(offset); });
    _mesh.onNodeDelayReceived([&](uint32_t nodeId, int32_t delay)
                                { this->delayReceivedCallback(nodeId, delay); });
}

void Mesh::delayCalc(const NodeList &nodes)
{
    if (_calcDelay)
    {
        for (NodeList::const_iterator node = nodes.begin(); node != nodes.end(); ++node)
        {
            _mesh.startDelayMeas(*node);
        }
        _calcDelay = false;
    }
}

// TODO: Need to parse the received message
// for some data:
//  - Last button pressed
//  - Name of current pattern
// Then pass data to Scarf so it can compute correct pattern

void Mesh::receivedCallback(uint32_t from, String &msg)
{
    Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, msg);

    for (const auto &observer : _receivedDataObservers)
    {
        observer(doc);
    }
}

void Mesh::newConnectionCallback(uint32_t nodeId)
{
    Serial.printf("--> startHere: New Connection, nodeId = %u, %s\n",
                    nodeId, _mesh.subConnectionJson(true).c_str());
}

void Mesh::printConnectionList(const NodeList &nodes)
{
    Serial.printf("Connection list (%d nodes):", nodes.size());

    for (NodeList::const_iterator node = nodes.begin(); node != nodes.end(); node++)
    {
        Serial.printf(" %u", *node);
    }
    Serial.println();
}

void Mesh::changedConnectionCallback()
{
    Serial.printf("Changed connections\n");
    NodeList nodes = _mesh.getNodeList();

    this->printConnectionList(nodes);
    _calcDelay = true;

    for (const auto &observer : _connectionObservers)
    {
        observer();
    }
}

void Mesh::nodeTimeAdjustedCallback(int32_t offset)
{
    Serial.printf("Adjusted time %ums. Offset = %d\n", this->getNodeTimeMs(), offset);
}

void Mesh::delayReceivedCallback(uint32_t from, int32_t delay)
{
    Serial.printf("Delay to node %u is %d us\n", from, delay);
}

uint32_t Mesh::getNodeTimeMs()
{
    const uint32_t k1024_Exp = 10;
    const int32_t nodeTimeMs = _mesh.getNodeTime() >> k1024_Exp;
    const int32_t kRolloverThresholdMs = 1000 * 1000;
    if (nodeTimeMs - _lastNodeTimeMs < -kRolloverThresholdMs)
    {
        _rolloverCount++;
        Serial.printf("getNodeTime() rollover!\n");
    }
    _lastNodeTimeMs = nodeTimeMs;
    return (nodeTimeMs & 0x003fffff | ((TimeMs)_rolloverCount << (32 - k1024_Exp)) & 0xffc00000);
}

} // namespace Scarfnet
