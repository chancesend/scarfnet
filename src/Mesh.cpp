#include "Mesh.h"

Mesh::Mesh(std::string ssid, std::string password, Scheduler* scheduler, uint16_t port) {
    _mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION | SYNC );  // set before init() so that you can see error messages
    _mesh.init(ssid.c_str(), password.c_str(), scheduler, port);
    _mesh.onReceive([&](uint32_t from, TSTRING &msg){ this->receivedCallback(from, msg); });
    _mesh.onNewConnection([&](uint32_t nodeId){ this->newConnectionCallback(nodeId); });
    _mesh.onChangedConnections([&](){ this->changedConnectionCallback(); });
    _mesh.onNodeTimeAdjusted([&](int32_t offset){ this->nodeTimeAdjustedCallback(offset); });
    _mesh.onNodeDelayReceived([&](uint32_t nodeId, int32_t delay){ this->delayReceivedCallback(nodeId, delay); });
}

void Mesh::delayCalc(const NodeList& nodes) {
    if (_calcDelay) {
        for (NodeList::const_iterator node = nodes.begin(); node != nodes.end(); ++node) {
            _mesh.startDelayMeas(*node);
        }
        _calcDelay = false;
    }
}

void Mesh::receivedCallback(uint32_t from, String & msg) {
    Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
}

void Mesh::newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u, %s\n", 
        nodeId, _mesh.subConnectionJson(true).c_str());

    _observers.onConnectionChange();
}

void Mesh::printConnectionList(const NodeList& nodes) {
    Serial.printf("Connection list (%d nodes):", nodes.size());

    for (NodeList::const_iterator node = nodes.begin(); node != nodes.end(); node++) {
        Serial.printf(" %u", *node);
    }
    Serial.println();
}

void Mesh::changedConnectionCallback() {
    Serial.printf("Changed connections\n");
    NodeList nodes = _mesh.getNodeList();

    this->printConnectionList(nodes);
    _calcDelay = true;
    
    _observers.onConnectionChange();
}

void Mesh::nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", this->getNodeTime(), offset);
}

void Mesh::delayReceivedCallback(uint32_t from, int32_t delay) {
    Serial.printf("Delay to node %u is %d us\n", from, delay);
}