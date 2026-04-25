#include "video_manager.hpp"

#include <iostream>
#include <chrono>

VideoManager::VideoManager(monitoring::HealthMonitor* monitor) 
    : server_(this)
    , healthMonitor_(monitor)
{
}

VideoManager::~VideoManager() {
    stop();
}

bool VideoManager::start(uint16_t port) {
    if (!server_->Start(nullptr, port)) {
        LOG_ERROR("video_manager", "Failed to start server on port ", port);
        if (healthMonitor_) {
            healthMonitor_->update("video_manager", false, "Start failed");
        }
        return false;
    }

    running_ = true;
    relayThread_ = std::thread(&VideoManager::relayThreadFunc, this);
    LOG_INFO("video_manager", "Started on port ", port);
    if (healthMonitor_) {
        healthMonitor_->update("video_manager", true, "Listening on port " + std::to_string(port));
    }
    return true;
}

void VideoManager::stop() {
    if (!running_) return;
    running_ = false;
    queueCv_.notify_all();
    if (relayThread_.joinable()) relayThread_.join();
    server_->Stop();
}

EnHandleResult VideoManager::OnAccept(ITcpServer* pSender, CONNID dwConnID, UINT_PTR soClient) {
    std::lock_guard<std::mutex> lk(clientsMutex_);
    VideoClient client{dwConnID, false};
    clients_[dwConnID] = client;
    LOG_INFO("video_manager", "Client connected: ", dwConnID);
    if (healthMonitor_) {
        healthMonitor_->update("video_manager", true, "Client connected: " + std::to_string(dwConnID));
    }
    return HR_OK;
}

EnHandleResult VideoManager::OnClose(ITcpServer* pSender, CONNID dwConnID, EnSocketOperation, int) {
    std::lock_guard<std::mutex> lk(clientsMutex_);
    clients_.erase(dwConnID);
    LOG_INFO("video_manager", "Client disconnected: ", dwConnID);
    if (healthMonitor_) {
        healthMonitor_->update("video_manager", true, "Client disconnected: " + std::to_string(dwConnID));
    }
    return HR_OK;
}

EnHandleResult VideoManager::OnReceive(ITcpServer* pSender, CONNID dwConnID, const BYTE* pData, int iLength) {
    if (iLength <= 0) return HR_OK;
    std::string payload(reinterpret_cast<const char*>(pData), iLength);

    // 处理角色声明
    if (payload.rfind("ROLE:", 0) == 0) {
        std::string role = payload.substr(5);
        std::lock_guard<std::mutex> lk(clientsMutex_);
        auto it = clients_.find(dwConnID);
        if (it != clients_.end()) {
            it->second.isPublisher = (role == "PUBLISHER");
            LOG_INFO("video_manager", "Client ", dwConnID, " role updated -> ", role);
        }
        return HR_OK;
    }

    {
        std::lock_guard<std::mutex> lk(clientsMutex_);
        if (auto it = clients_.find(dwConnID); it != clients_.end() && !it->second.isPublisher) {
            LOG_WARN("video_manager", "Subscriber ", dwConnID, " attempted to push data. Ignored.");
            return HR_OK;
        }
    }

    VideoPacket pkt;
    pkt.data.assign(pData, pData + iLength);
    pkt.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        packetQueue_.push(pkt);
    }
    queueCv_.notify_one();

    return HR_OK;
}

void VideoManager::relayThreadFunc() {
    while (running_) {
        std::unique_lock<std::mutex> lk(queueMutex_);
        queueCv_.wait(lk, [this]() { return !packetQueue_.empty() || !running_; });
        if (!running_) break;

        VideoPacket pkt = std::move(packetQueue_.front());
        packetQueue_.pop();
        lk.unlock();

        // 广播给所有客户端
        std::lock_guard<std::mutex> lk2(clientsMutex_);
        for (auto& [id, client] : clients_) {
            if (!client.isPublisher) {
                server_->Send(id, pkt.data.data(), static_cast<int>(pkt.data.size()));
            }
        }

        if (healthMonitor_) {
            healthMonitor_->update("video_manager", true, "Video packet broadcast");
        }
    }
}
