#pragma once

#include "HPSocket.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <mutex>

class ServerListener : public CTcpServerListener
{
private:
    CONNID _connID = 0;
    std::vector<CONNID> _connIDs;
    mutable std::mutex _connMutex;

public:
    EnHandleResult OnPrepareListen(ITcpServer *pSender, SOCKET soListen) override
    {
        // std::cout << "[Server] Ready to listen..." << std::endl;
        return HR_OK;
    }

    EnHandleResult OnAccept(ITcpServer *pSender, CONNID dwConnID, UINT_PTR soClient) override
    {
        _connID = dwConnID;
        {
            std::lock_guard<std::mutex> lk(_connMutex);
            _connIDs.push_back(dwConnID);
        }

        std::cout << "[Server] Client connected: " << dwConnID << std::endl;
        return HR_OK;
    }

    EnHandleResult OnReceive(ITcpServer *pSender, CONNID dwConnID, const BYTE *pData, int iLength) override
    {
        std::string msg((const char *)pData, iLength);
        std::cout << "[Server] Received: " << msg << std::endl;

        std::string reply = "Server reply: " + msg;
        pSender->Send(dwConnID, (const BYTE *)reply.c_str(), reply.size());
        return HR_OK;
    }

    EnHandleResult OnClose(ITcpServer *pSender, CONNID dwConnID, EnSocketOperation, int iErrorCode) override
    {
        std::cout << "[Server] Client disconnected: " << dwConnID << std::endl;
        std::lock_guard<std::mutex> lk(_connMutex);
        _connIDs.erase(
            std::remove(_connIDs.begin(), _connIDs.end(), dwConnID),
            _connIDs.end());
        return HR_OK;
    }


    CONNID 
    getConnectionID() const 
    { return _connID; }

    std::vector<CONNID> 
    getAllConnectionIDs() const 
    { 
        std::lock_guard<std::mutex> lk(_connMutex);
        return _connIDs; 
    }

    bool hasConnections() const
    {
        std::lock_guard<std::mutex> lk(_connMutex);
        return !_connIDs.empty();
    }

    template <typename Fn>
    void forEachConnection(Fn&& fn) const
    {
        auto ids = getAllConnectionIDs();
        for (const auto id : ids) {
            fn(id);
        }
    }
};
