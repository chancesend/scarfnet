#include "Mesh.h"
#include "log.h"

#include <unordered_set>

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
        Serial.printf("###NEW CONNECTION!!\n");
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
    Serial.printf("Mesh::Mesh\n");
}

void Mesh::update()
{
    _mesh.update();
}

void Mesh::delayCalc(NodeList &nodes) 
{
    if (_calcDelay)
    {
        for (auto node = nodes.begin(); node != nodes.end(); node++)
        {
            auto isNodeIdConnected = _mesh.startDelayMeas(*node);
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
    Serial.printf("[MESH][RCV node %u] msg=%s\n", from, msg.c_str());
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, msg);

    for (const auto& observer : _receivedDataObservers)
    {
        observer(doc);
    }
}

void Mesh::newConnectionCallback(uint32_t nodeId)
{
    auto jsonLayout = _mesh.subConnectionJson(true);
    Serial.printf("[MESH][NEW node %u] %s\n",
                    nodeId, jsonLayout.c_str());

    // Add to our local node list
    _nodes.push_back(nodeId);
    
    // Trigger connection change notification
    onConnectionChange();
}

void Mesh::droppedConnectionCallback(uint32_t nodeId)
{
    auto jsonLayout = _mesh.subConnectionJson(true);
    Serial.printf("[MESH][DROP node %u] %s\n",
                    nodeId, jsonLayout.c_str());

    // Remove from our local node list
    _nodes.remove(nodeId);
    
    // Trigger connection change notification
    onConnectionChange();
}

void Mesh::printConnectionList(NodeList &nodes)
{
    Serial.printf("Connection list (%d nodes):", nodes.size());

    for (auto node = nodes.begin(); node != nodes.end(); node++)
    {
        Serial.printf(" %u", *node);
    }
    Serial.printf("\n");
}

void Mesh::cleanupDisconnectedNodes()
{
    TimeMs currentTime = millis();
    if (currentTime - _lastCleanupTime < CLEANUP_INTERVAL_MS) {
        return; // Don't cleanup too frequently
    }
    
    _lastCleanupTime = currentTime;
    
    // Get current mesh node list
    auto meshNodes = _mesh.getNodeList();
    
    // Create a set of current active node IDs for fast lookup
    std::unordered_set<uint32_t> activeNodes;
    for (auto nodeId : meshNodes) {
        activeNodes.insert(nodeId);
    }
    
    // Remove nodes that are no longer in the mesh
    NodeList::iterator it = _nodes.begin();
    while (it != _nodes.end()) {
        if (activeNodes.find(*it) == activeNodes.end()) {
            // Node is no longer connected, remove it
            it = _nodes.erase(it);
        } else {
            ++it;
        }
    }
}

void Mesh::changedConnectionCallback()
{
    Serial.printf("[MESH] Changed connections\n");
    NodeList nodes = _mesh.getNodeList();

    this->printConnectionList(nodes);
    _calcDelay = true;

    for (const auto& observer : _connectionObservers)
    {
        observer();
    }
}

void Mesh::nodeTimeAdjustedCallback(int32_t offset)
{
    Serial.printf("[MESH] Adjusted time %ums. Offset = %d\n", this->getNodeTimeMs(), offset);
}

void Mesh::delayReceivedCallback(uint32_t from, int32_t delay)
{
    Serial.printf("[MESH] Delay to node %u is %d us\n", from, delay);
}

uint32_t Mesh::getMeshNodeTimeRaw()
{
    return _mesh.getNodeTime();
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
