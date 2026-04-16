#pragma once

#include <painlessMesh.h>

#include <ArduinoJson.h>

#include <vector>
#include <map>
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

    // Delegates to Scarfnet::computeNodeTimeMs (see mesh_time.h). Kept as a
    // static method for call sites that already have a Mesh instance.
    static uint32_t computeNodeTimeMs(uint32_t rawNodeTime, int32_t& lastNodeTimeMs, int32_t& rolloverCount);

    // Returns the number of distinct nodes in the mesh, including this node.
    // sort()+unique() guards against transient duplicate nodeIds that painlessMesh
    // can produce during topology churn (two neighbours briefly claiming the same
    // remote node as their subtree descendant).
    int getNumNodes()
    {
        auto nodes = _mesh.getNodeList();
        nodes.sort();
        nodes.unique();
        return (int)nodes.size() + 1;
    }

    bool sendBroadcast(TSTRING msg, bool includeSelf = false)
    {
        return _mesh.sendBroadcast(msg, includeSelf);
    };

    // Records the one-way arrival delta (receiverTimeMs - senderTimeMs) for a
    // node and updates its EMA-smoothed estimate. Called on every heartbeat.
    void recordArrivalDelta(uint32_t nodeId, int32_t rawDeltaMs);

    // Returns the EMA-smoothed arrival delta for a node, or 0 if unknown.
    int32_t getArrivalDelta(uint32_t nodeId) const;

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

    // EMA-smoothed one-way arrival delta per peer node (ms). Populated by
    // recordArrivalDelta() and used to estimate "network distance" for swarm patterns.
    std::map<uint32_t, int32_t> _nodeArrivalDeltas;

    typedef std::vector<ConnectionCallback_t> ConnectionObserverList;
    ConnectionObserverList _connectionObservers;

    typedef std::vector<ReceivedDataCallback_t> ReceivedDataObserverList;
    ReceivedDataObserverList _receivedDataObservers;
};

}
