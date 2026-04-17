#include "Mesh.h"
#include "config.h"
#include "mesh_time.h"
#include "swarm_ema.h"
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
            Scarfnet::log("[MESH] NEW CONNECTION!!");
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
    Scarfnet::log("Mesh::Mesh");
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
    Scarfnet::log("[MESH][RCV node %u] msg=%s", from, msg.c_str());
    JsonDocument doc;
    auto err = deserializeJson(doc, msg);
    if (err)
    {
        Scarfnet::log("[MESH][RCV] JSON parse error: %s", err.c_str());
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
    Scarfnet::log("[MESH][NEW node %u] %s", nodeId, jsonLayout.c_str());
    // changedConnectionCallback fires immediately after and notifies observers
}

void Mesh::droppedConnectionCallback(uint32_t nodeId)
{
    auto jsonLayout = _mesh.subConnectionJson(true);
    Scarfnet::log("[MESH][DROP node %u] %s", nodeId, jsonLayout.c_str());
    // changedConnectionCallback fires immediately after and notifies observers
}

void Mesh::printConnectionList()
{
    auto nodes = _mesh.getNodeList();
    nodes.sort();
    nodes.unique();
    String line = String("Connection list (") + nodes.size() + " nodes):";
    for (auto node : nodes)
        line += String(" ") + node;
    Scarfnet::log(line.c_str());
}

void Mesh::changedConnectionCallback()
{
    Scarfnet::log("[MESH] Changed connections");
    this->printConnectionList();
    _calcDelay = true;

    for (const auto& observer : _connectionObservers)
    {
        observer();
    }
}

void Mesh::nodeTimeAdjustedCallback(int32_t offset)
{
    Scarfnet::log("[MESH] Adjusted time %ums. Offset = %d", this->getNodeTimeMs(), offset);
}

void Mesh::delayReceivedCallback(uint32_t from, int32_t delay)
{
    Scarfnet::log("[MESH] Delay to node %u is %d us", from, delay);
}

uint32_t Mesh::getMeshNodeTimeRaw()
{
    return _mesh.getNodeTime();
}

/*static*/ uint32_t Mesh::computeNodeTimeMs(uint32_t rawNodeTime, int32_t& lastNodeTimeMs, int32_t& rolloverCount)
{
    return Scarfnet::computeNodeTimeMs(rawNodeTime, lastNodeTimeMs, rolloverCount);
}

uint32_t Mesh::getNodeTimeMs()
{
    return computeNodeTimeMs(_mesh.getNodeTime(), _lastNodeTimeMs, _rolloverCount);
}

void Mesh::recordArrivalDelta(uint32_t nodeId, int32_t rawDeltaMs)
{
    if (!swarmDeltaIsPlausible(rawDeltaMs, kSwarmMaxArrivalDeltaMs))
    {
        Scarfnet::log("[SWARM] node %u delta: raw=%dms DISCARDED (outside ±%dms window)",
                      nodeId, rawDeltaMs, kSwarmMaxArrivalDeltaMs);
        return;
    }

    // Seed new entries at 0 so the first sample is weighted by alpha rather
    // than replacing the stored value outright (see swarm_ema.h).
    auto result = _nodeArrivalDeltas.emplace(nodeId, int32_t{0});
    auto it = result.first;
    if (result.second)
        Scarfnet::log("[SWARM] node %u first delta: %dms", nodeId, rawDeltaMs);

    int32_t smoothed = swarmEmaUpdate(rawDeltaMs, it->second);
    Scarfnet::log("[SWARM] node %u delta: raw=%dms smoothed=%dms", nodeId, rawDeltaMs, smoothed);
    it->second = smoothed;
}

int32_t Mesh::getArrivalDelta(uint32_t nodeId) const
{
    auto it = _nodeArrivalDeltas.find(nodeId);
    return (it != _nodeArrivalDeltas.end()) ? it->second : 0;
}

} // namespace Scarfnet
