#pragma once
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <memory>
#include <string>
#include <cstdint>
#include <unordered_map>

#include "HPSocket.h"  // 假设 HPSocket 已包含路径
#include "core/logger.hpp"
#include "monitoring/health_monitor.hpp"

// 视频数据包结构
struct VideoPacket {
    std::vector<uint8_t> data;
    int64_t timestamp;
};

// 视频客户端结构
struct VideoClient {
    CONNID id;
    bool isPublisher; // true=推流客户端, false=订阅客户端
};

// 视频中转管理类
class VideoManager : public CTcpServerListener {
public:
    explicit VideoManager(monitoring::HealthMonitor* monitor = nullptr);
    ~VideoManager();

    bool start(uint16_t port);
    void stop();

    void setHealthMonitor(monitoring::HealthMonitor* monitor) { healthMonitor_ = monitor; }

    // 推流客户端收到数据
    EnHandleResult OnReceive(ITcpServer* pSender, CONNID dwConnID, const BYTE* pData, int iLength) override;
    // 客户端连接/断开
    EnHandleResult OnAccept(ITcpServer* pSender, CONNID dwConnID, UINT_PTR soClient) override;
    EnHandleResult OnClose(ITcpServer* pSender, CONNID dwConnID, EnSocketOperation enOperation, int iErrorCode) override;

private:
    void relayThreadFunc();

private:
    CTcpServerPtr server_;
    std::thread relayThread_;
    std::atomic<bool> running_{false};

    std::mutex clientsMutex_;
    std::unordered_map<CONNID, VideoClient> clients_;

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::queue<VideoPacket> packetQueue_;
    monitoring::HealthMonitor* healthMonitor_{nullptr};
};
