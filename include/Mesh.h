#pragma once

#include <painlessMesh.h>

#include <ArduinoJson.h>

#include <vector>
#include <memory>
#include <functional>
#include <stdint.h>

#include "log.h"

namespace Scarfnet
{

struct MeshConnection
{
    std::string ssid;
    std::string password;
    uint16_t    port;
};

// Wraps painlessMesh and exposes observer hooks for connection events and
// incoming JSON messages. Also provides a synchronized millisecond timestamp
// with rollover protection.
class Mesh
{
public:
    typedef std::shared_ptr<Mesh> Ptr;
    typedef std::function<void()> ConnectionCallback_t;
    typedef std::function<void(const JsonDocument&)> ReceivedDataCallback_t;
    typedef int32_t TimeMs;

    Mesh(MeshConnection connection, Scheduler* scheduler) :
        Mesh(connection.ssid, connection.password, scheduler, connection.port)
    {
    }

    Mesh(std::string ssid, std::string password, Scheduler* scheduler, uint16_t port);

    // Must be called every loop iteration to drive the mesh and TaskScheduler.
    void update();

    uint32_t getNodeId() { return _mesh.getNodeId(); };

    uint32_t getMeshNodeTimeRaw();
    // Returns a mesh-synchronized millisecond timestamp with rollover protection.
    uint32_t getNodeTimeMs();

    // Extracted for unit testing: converts a raw painlessMesh node time
    // (microseconds, uint32_t) to a millisecond timestamp with rollover tracking.
    static uint32_t computeNodeTimeMs(uint32_t rawNodeTime, int32_t& lastNodeTimeMs, int32_t& rolloverCount);

    // Returns the number of nodes in the mesh, including this node.
    int getNumNodes()
    {
        return (_mesh.getNodeList().size() + 1);
    }

    bool sendBroadcast(TSTRING msg, bool includeSelf = false)
    {
        return _mesh.sendBroadcast(msg, includeSelf);
    };

    // Triggers a one-shot delay calculation to estimate link latency.
    void doDelayCalc()
    {
        delayCalc();
    }

    // Registers a callback invoked whenever the mesh topology changes.
    void addConnectionObserver(const ConnectionCallback_t& observer)
    {
        Scarfnet::log("[MESH] registering connection observer\n");
        _connectionObservers.push_back(observer);
    }
    void onConnectionChange()
    {
        Scarfnet::log("[MESH] onConnectionChange()\n");
        for(const auto& observer: _connectionObservers)
        {
            observer();
        }
    }

    // Registers a callback invoked whenever a JSON broadcast is received.
    void addReceivedDataObserver(const ReceivedDataCallback_t& observer)
    {
        _receivedDataObservers.push_back(observer);
    }
    void onReceivedData(const JsonDocument& doc)
    {
        for(const auto& observer: _receivedDataObservers)
        {
            observer(doc);
        }
    }

private:
    void receivedCallback(uint32_t from, String & msg);
    void newConnectionCallback(uint32_t nodeId);
    void droppedConnectionCallback(uint32_t nodeId);
    void changedConnectionCallback();
    void nodeTimeAdjustedCallback(int32_t offset);
    void delayReceivedCallback(uint32_t from, int32_t delay);

    void delayCalc();
    void printConnectionList();

    bool _calcDelay {false};
    painlessmesh::wifi::Mesh    _mesh;
    TimeMs   _lastNodeTimeMs {0};
    int32_t     _rolloverCount {0};

    typedef std::vector<ConnectionCallback_t> ConnectionObserverList;
    ConnectionObserverList _connectionObservers;

    typedef std::vector<ReceivedDataCallback_t> ReceivedDataObserverList;
    ReceivedDataObserverList _receivedDataObservers;
};

}
