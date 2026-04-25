#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <arpa/inet.h>

#include "core/configuration.hpp"
#include "core/logger.hpp"
#include "domain/telemetry_models.hpp"
#include "monitoring/health_monitor.hpp"
#include "transport/sensor_data_settings.hpp"
#include "network/server_listener_tcp.hpp"

class TelemetryPublisher : public ServerListener {
public:
    using SnapshotProvider = std::function<std::vector<domain::TelemetryFrame>()>;

    TelemetryPublisher(const core::PublisherConfig& config,
                       DeviceCommandRouter& router,
                       monitoring::HealthMonitor& monitor)
        : config_(config)
        , router_(router)
        , monitor_(monitor)
        , server_(this) {
    }

    bool start() {
        server_->SetMaxConnectionCount(config_.maxConnections);
        server_->SetWorkerThreadCount(config_.workerThreads);
        if (!server_->Start(config_.bindAddress.c_str(), config_.port)) {
            LOG_ERROR("telemetry_publisher", "Failed to start server on ", config_.bindAddress, ":", config_.port);
            return false;
        }
        monitor_.update("telemetry_publisher", true, "Server listening");
        LOG_INFO("telemetry_publisher", "Listening on ", config_.bindAddress, ":", config_.port);
        return true;
    }

    void stop() {
        server_->Stop();
        monitor_.update("telemetry_publisher", false, "Server stopped");
    }

    bool hasSubscribers() const {
        return server_->GetConnectionCount() > 0;
    }

    void publish(const domain::TelemetryFrame& frame) {
        if (!hasSubscribers()) {
            return;
        }

        auto payload = domain::toJson(frame).dump();
        std::vector<uint8_t> buffer(sizeof(uint32_t) + payload.size());
        uint32_t len = static_cast<uint32_t>(payload.size());
        uint32_t netLen = htonl(len);
        std::memcpy(buffer.data(), &netLen, sizeof(uint32_t));
        std::memcpy(buffer.data() + sizeof(uint32_t), payload.data(), payload.size());

        forEachConnection([&](CONNID id) {
            server_->Send(id, buffer.data(), static_cast<int>(buffer.size()));
        });

        monitor_.update("telemetry_publisher", true, "Frame delivered to clients");
    }

    void setSnapshotProvider(SnapshotProvider provider) {
        snapshotProvider_ = std::move(provider);
    }

protected:
    EnHandleResult OnAccept(ITcpServer* pSender, CONNID dwConnID, UINT_PTR soClient) override {
        auto result = ServerListener::OnAccept(pSender, dwConnID, soClient);
        monitor_.update("telemetry_publisher", true, "Client connected: " + std::to_string(dwConnID));
        if (snapshotProvider_) {
            for (const auto& frame : snapshotProvider_()) {
                publish(frame);
            }
        }
        return result;
    }

    EnHandleResult OnClose(ITcpServer* pSender, CONNID dwConnID, EnSocketOperation op, int errorCode) override {
        monitor_.update("telemetry_publisher", true, "Client disconnected: " + std::to_string(dwConnID));
        return ServerListener::OnClose(pSender, dwConnID, op, errorCode);
    }

    EnHandleResult OnReceive(ITcpServer* pSender, CONNID dwConnID, const BYTE* pData, int iLength) override {
        std::string chunk(reinterpret_cast<const char*>(pData), iLength);
        router_.feed(static_cast<uint64_t>(dwConnID), chunk, [&](const std::string& reply) {
            auto payload = reply + "\n";
            pSender->Send(dwConnID, reinterpret_cast<const BYTE*>(payload.data()), static_cast<int>(payload.size()));
        });
        return HR_OK;
    }

private:
    core::PublisherConfig config_;
    DeviceCommandRouter& router_;
    monitoring::HealthMonitor& monitor_;
    SnapshotProvider snapshotProvider_;
    CTcpServerPtr server_;
};
