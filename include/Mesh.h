#pragma once

#include <painlessMesh.h>

#include <ArduinoJson.h>

#include <vector>

namespace Scarfnet
{

class Mesh
{
public:
    typedef std::function<void()> ConnectionCallback_t;
    typedef std::function<void(const DynamicJsonDocument&)> ReceivedDataCallback_t;

    typedef int32_t TimeMs;
    
    Mesh(std::string ssid, std::string password, Scheduler* scheduler, uint16_t port);

    void update() { return _mesh.update(); };
    uint32_t getNodeId() { return _mesh.getNodeId(); };

    uint32_t getNodeTimeMs();

    int getNumNodes()
    {
        return (_mesh.getNodeList().size() + 1);
    }

    bool sendBroadcast(TSTRING msg, bool includeSelf = false) 
    { 
        return _mesh.sendBroadcast(msg, includeSelf); 
    };

    void doDelayCalc() 
    { 
        delayCalc(_nodes); 
    }
    
    void receivedCallback(uint32_t from, String & msg);
    void newConnectionCallback(uint32_t nodeId);
    void changedConnectionCallback(); 
    void nodeTimeAdjustedCallback(int32_t offset); 
    void delayReceivedCallback(uint32_t from, int32_t delay);

    void addConnectionObserver(const ConnectionCallback_t& observer)
    {
        _connectionObservers.push_back(observer);
    }
    void onConnectionChange()
    {
        for(const auto& observer: _connectionObservers)
        {
            observer();
        }
    }

    void addReceivedDataObserver(const ReceivedDataCallback_t& observer)
    {
        _receivedDataObservers.push_back(observer);
    }
    void onReceivedData(const DynamicJsonDocument& doc)
    {
        for(const auto& observer: _receivedDataObservers)
        {
            observer(doc);
        }
    }

private:
    typedef SimpleList<uint32_t> NodeList;
    NodeList _nodes;

    void delayCalc(const NodeList& nodes);
    void printConnectionList(const NodeList& nodes);

    bool _calcDelay {false};
    painlessMesh    _mesh;
    TimeMs   _lastNodeTimeMs {0};
    int32_t     _rolloverCount {0};
    
    typedef std::vector<ConnectionCallback_t> ConnectionObserverList;
    ConnectionObserverList _connectionObservers;
    typedef std::vector<ReceivedDataCallback_t> ReceivedDataObserverList;
    ReceivedDataObserverList _receivedDataObservers;
};

}
