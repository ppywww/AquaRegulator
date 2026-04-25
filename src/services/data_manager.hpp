#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "core/configuration.hpp"
#include "domain/telemetry_models.hpp"
#include "infrastructure/cache/telemetry_cache.hpp"
#include "infrastructure/database/telemetry_repository.hpp"
#include "monitoring/health_monitor.hpp"
#include "infrastructure/sensors/sensor_data.hpp"
#include "transport/tcp_data_sender.hpp"

class TelemetryService {//遥测数据管理服务
public:
    TelemetryService(const core::PipelineConfig& pipelineConfig,
                     infrastructure::database::TelemetryRepository& repository,
                     SensorGateway& sensorGateway,
                     TelemetryPublisher& publisher,
                     monitoring::HealthMonitor& healthMonitor)
        : pipelineConfig_(pipelineConfig)
        , repository_(repository)
        , sensorGateway_(sensorGateway)
        , publisher_(publisher)
        , healthMonitor_(healthMonitor)
        , cache_(pipelineConfig.cacheSize) {
        publisher_.setSnapshotProvider([this]() {//设置发布器的快照提供函数，用于提供实时数据和历史数据
            std::vector<domain::TelemetryFrame> frames;
            frames.push_back(buildSnapshot(domain::TelemetryChannel::Realtime));
            frames.push_back(buildSnapshot(domain::TelemetryChannel::HistoricalEnvironment));
            frames.push_back(buildSnapshot(domain::TelemetryChannel::HistoricalSoil));
            return frames;
        });
    }

    ~TelemetryService() {
        stop();
    }

    void start() {
        if (running_.exchange(true)) {
            return;
        }
        worker_ = std::thread(&TelemetryService::runLoop, this);
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }
        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    void runLoop() {
        using namespace std::chrono;//使用chrono命名空间，方便使用时间相关的函数
        auto lastHistorical = steady_clock::now() - seconds(pipelineConfig_.historicalIntervalSeconds);

        while (running_) {
            auto start = steady_clock::now();
            processRealtime();
            //如果距离上次处理历史数据的时间超过间隔，处理历史数据 60秒
            if (steady_clock::now() - lastHistorical >= seconds(pipelineConfig_.historicalIntervalSeconds)) {
                processHistorical();//处理历史数据
                lastHistorical = steady_clock::now();
            }
            //等待剩余时间，确保实时数据处理间隔为1秒
            auto elapsed = steady_clock::now() - start;
            auto waitTime = seconds(pipelineConfig_.realtimeIntervalSeconds) - elapsed;
            if (waitTime > seconds(0)) {//如果等待时间大于0，等待剩余时间
                std::this_thread::sleep_for(waitTime);
            }
        }
    }

    void processRealtime() {
        auto reading = sensorGateway_.readRealtime();//读实时数据
        if (!reading.has_value()) {
            healthMonitor_.update("telemetry_service", false, "Realtime read failed");
            return;
        }

        // 保存实时数据到数据库
        if (!repository_.saveRealtime(*reading)) {
            LOG_WARN("telemetry_service", "Failed to save realtime data to database");
            healthMonitor_.update("telemetry_service", false, "Failed to save data to database");
        } else {
            healthMonitor_.update("telemetry_service", true, "Data saved to database");
        }

        cache_.store(domain::TelemetryChannel::Realtime, *reading);//缓存实时数据
        
        //发布实时数据  
        if (publisher_.hasSubscribers()) {
            domain::TelemetryFrame frame;
            frame.channel = domain::TelemetryChannel::Realtime;
            frame.snapshot = false;
            frame.correlationId = nextCorrelationId();
            frame.readings.push_back(*reading);
            publisher_.publish(frame);
        }

        healthMonitor_.update("telemetry_service", true, "Realtime frame published");
    }

    void processHistorical() {
        auto env = repository_.loadEnvironmental(pipelineConfig_.cacheSize);
        auto soil = repository_.loadSoilAndAir(pipelineConfig_.cacheSize);

        for (const auto& reading : env) {
            cache_.store(domain::TelemetryChannel::HistoricalEnvironment, reading);
        }
        for (const auto& reading : soil) {
            cache_.store(domain::TelemetryChannel::HistoricalSoil, reading);
        }

        if (publisher_.hasSubscribers()) {
            if (!env.empty()) {
                auto frame = buildFrame(domain::TelemetryChannel::HistoricalEnvironment, env);
                publisher_.publish(frame);
            }
            if (!soil.empty()) {
                auto frame = buildFrame(domain::TelemetryChannel::HistoricalSoil, soil);
                publisher_.publish(frame);
            }
        }

        healthMonitor_.update("telemetry_service", true, "Historical frame published");
    }

    domain::TelemetryFrame buildSnapshot(domain::TelemetryChannel channel) const {
        auto readings = cache_.snapshot(channel);
        return buildFrame(channel, readings);
    }
    //构建遥测数据帧
    //每个遥测数据帧包含通道、读取时间戳、数据长度、数据本身
    domain::TelemetryFrame buildFrame(domain::TelemetryChannel channel,
                                      const std::vector<domain::TelemetryReading>& readings) const {
        domain::TelemetryFrame frame;
        frame.channel = channel;
        frame.readings = readings;
        frame.snapshot = true;
        frame.correlationId = nextCorrelationId();
        return frame;
    }
    //生成下一个关联ID
    //每个遥测数据帧都有一个唯一的关联ID，用于标识该帧
    //关联ID用于在客户端和服务器之间进行通信，确保数据的正确性和一致性
    std::string nextCorrelationId() const {
        auto id = ++correlationId_;
        return "frame-" + std::to_string(id);
    }

    core::PipelineConfig pipelineConfig_;
    infrastructure::database::TelemetryRepository& repository_;
    SensorGateway& sensorGateway_;
    TelemetryPublisher& publisher_;
    monitoring::HealthMonitor& healthMonitor_;
    infrastructure::cache::TelemetryCache cache_;

    std::atomic<bool> running_{false};
    std::thread worker_;
    mutable std::atomic<uint64_t> correlationId_{0};
};
