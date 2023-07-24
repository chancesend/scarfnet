#pragma once

#include <painlessMesh.h>

#include <vector>

namespace Scarf
{

class Mesh
{
public:
    typedef std::function<void()> ConnectionCallback_t;
    
    Mesh(std::string ssid, std::string password, Scheduler* scheduler, uint16_t port);

    void update() { return _mesh.update(); };
    uint32_t getNodeId() { return _mesh.getNodeId(); };

    uint32_t getNodeTime() { return _mesh.getNodeTime(); };
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
        _observers.push_back(observer);
    }
    void onConnectionChange()
    {
        for(const auto& observer: _observers)
        {
            observer();
        }
    }

private:
    typedef SimpleList<uint32_t> NodeList;
    NodeList _nodes;

    void delayCalc(const NodeList& nodes);
    void printConnectionList(const NodeList& nodes);

    bool _calcDelay {false};
    painlessMesh    _mesh;
    
    typedef std::vector<ConnectionCallback_t> ObserverList;
    ObserverList _observers;
};

}
