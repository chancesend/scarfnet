#include "Mesh.h"
#include "log.h"

namespace Scarfnet
{

Mesh::Mesh(std::string ssid, std::string password, Scheduler *scheduler, uint16_t port)
{
    
    _mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION | MESH_STATUS | SYNC); // set before init() so that you can see error messages
    _mesh.init(ssid.c_str(), password.c_str(), scheduler, port);

    _mesh.onReceive([&](uint32_t from, TSTRING &msg)
        { this->receivedCallback(from, msg); });
    _mesh.onNewConnection([&](uint32_t nodeId)
        {
            Scarfnet::log("###NEW CONNECTION!!\n");
            this->newConnectionCallback(nodeId);
            });
    _mesh.onDroppedConnection([&](uint32_t nodeId) 
        { this->droppedConnectionCallback(nodeId); });
    _mesh.onChangedConnections([&]()
        { this->changedConnectionCallback(); });
    _mesh.onNodeTimeAdjusted([&](int32_t offset)
        { this->nodeTimeAdjustedCallback(offset); });
    _mesh.onNodeDelayReceived([&](uint32_t nodeId, int32_t delay)
        { this->delayReceivedCallback(nodeId, delay); });
    Scarfnet::log("Mesh::Mesh\n");
}

void Mesh::update()
{
    _mesh.update();
}

void Mesh::delayCalc()
{
    if (_calcDelay)
    {
        auto nodes = _mesh.getNodeList();
        for (auto node : nodes)
        {
            _mesh.startDelayMeas(node);
        }
        _calcDelay = false;
    }
}

void Mesh::receivedCallback(uint32_t from, String &msg)
{
    Scarfnet::log("[MESH][RCV node %u] msg=%s\n", from, msg.c_str());
    JsonDocument doc;
    auto err = deserializeJson(doc, msg);
    if (err)
    {
        Scarfnet::log("[MESH][RCV] JSON parse error: %s\n", err.c_str());
        return;
    }

    for (const auto& observer : _receivedDataObservers)
    {
        observer(doc);
    }
}

void Mesh::newConnectionCallback(uint32_t nodeId)
{
    auto jsonLayout = _mesh.subConnectionJson(true);
    Scarfnet::log("[MESH][NEW node %u] %s\n", nodeId, jsonLayout.c_str());
    // changedConnectionCallback fires immediately after and notifies observers
}

void Mesh::droppedConnectionCallback(uint32_t nodeId)
{
    auto jsonLayout = _mesh.subConnectionJson(true);
    Scarfnet::log("[MESH][DROP node %u] %s\n", nodeId, jsonLayout.c_str());
    // changedConnectionCallback fires immediately after and notifies observers
}

void Mesh::printConnectionList()
{
    auto nodes = _mesh.getNodeList();
    Scarfnet::log("Connection list (%d nodes):", nodes.size());
    for (auto node : nodes)
    {
        Scarfnet::log(" %u", node);
    }
    Scarfnet::log("\n");
}

void Mesh::changedConnectionCallback()
{
    Scarfnet::log("[MESH] Changed connections\n");
    this->printConnectionList();
    _calcDelay = true;

    for (const auto& observer : _connectionObservers)
    {
        observer();
    }
}

void Mesh::nodeTimeAdjustedCallback(int32_t offset)
{
    Scarfnet::log("[MESH] Adjusted time %ums. Offset = %d\n", this->getNodeTimeMs(), offset);
}

void Mesh::delayReceivedCallback(uint32_t from, int32_t delay)
{
    Scarfnet::log("[MESH] Delay to node %u is %d us\n", from, delay);
}

uint32_t Mesh::getMeshNodeTimeRaw()
{
    return _mesh.getNodeTime();
}

/*static*/ uint32_t Mesh::computeNodeTimeMs(uint32_t rawNodeTime, int32_t& lastNodeTimeMs, int32_t& rolloverCount)
{
    const uint32_t kShift = 10; // divide microseconds by 1024 ≈ milliseconds
    const int32_t nodeTimeMs = (int32_t)(rawNodeTime >> kShift);
    const int32_t kRolloverThresholdMs = 1000 * 1000;
    if (nodeTimeMs - lastNodeTimeMs < -kRolloverThresholdMs)
    {
        rolloverCount++;
        Scarfnet::log("getNodeTime() rollover!\n");
    }
    lastNodeTimeMs = nodeTimeMs;
    return ((uint32_t)nodeTimeMs & 0x003fffffu) | (((uint32_t)rolloverCount << (32u - kShift)) & 0xffc00000u);
}

uint32_t Mesh::getNodeTimeMs()
{
    return computeNodeTimeMs(_mesh.getNodeTime(), _lastNodeTimeMs, _rolloverCount);
}

} // namespace Scarfnet
