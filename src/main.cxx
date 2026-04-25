#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include "core/configuration.hpp"
#include "core/logger.hpp"
#include "services/data_manager.hpp"
#include "monitoring/health_monitor.hpp"
#include "infrastructure/sensors/sensor_data.hpp"
#include "transport/sensor_data_settings.hpp"
#include "transport/tcp_data_sender.hpp"
#include "services/transport/video_manager.hpp"
#include "infrastructure/database/telemetry_repository.hpp"

namespace {
std::atomic<bool>* g_shouldRun = nullptr;

void handleSignal(int) {
    if (g_shouldRun) {
        g_shouldRun->store(false);
    }
}
}

int main() {
    core::Logger::instance().configure(core::LogLevel::Info, "logs/aqua_regulator.log");
    core::ConfigurationManager configManager("config/app_config.json");
    const auto& config = configManager.get();

    monitoring::HealthMonitor healthMonitor(config.health.statusFile,
                                            std::chrono::seconds(config.health.intervalSeconds));
    healthMonitor.start();

    infrastructure::database::TelemetryRepository repository;
    if (!repository.initialize(config.database)) {
        LOG_CRITICAL("bootstrap", "Failed to connect to database. Exiting.");
        return EXIT_FAILURE;
    }

    SensorGateway sensorGateway(config.sensor, healthMonitor);
    std::atomic<bool> reloadRequested{false};

    TelemetryPublisher* publisherPtr = nullptr;
    DeviceCommandRouter router(
        sensorGateway,
        healthMonitor,
        [&]() {
            nlohmann::json json;
            json["telemetry"]["subscribers"] = publisherPtr ? publisherPtr->hasSubscribers() : false;
            json["pipeline"]["realtimeSeconds"] = config.pipeline.realtimeIntervalSeconds;
            json["pipeline"]["historicalSeconds"] = config.pipeline.historicalIntervalSeconds;
            return json;
        },
        [&]() {
            reloadRequested.store(true);
        });

    TelemetryPublisher publisher(config.publisher, router, healthMonitor);
    publisherPtr = &publisher;
    if (!publisher.start()) {
        LOG_CRITICAL("bootstrap", "Failed to start telemetry publisher");
        return EXIT_FAILURE;
    }

    TelemetryService telemetryService(config.pipeline, repository, sensorGateway, publisher, healthMonitor);
    telemetryService.start();

    VideoManager videoManager(&healthMonitor);
    if (!videoManager.start(config.video.port)) {
        LOG_WARN("bootstrap", "Video manager failed to start");
    }

    std::atomic<bool> shouldRun{true};
    g_shouldRun = &shouldRun;
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    LOG_INFO("bootstrap", "AquaRegulator backend is running");

    while (shouldRun.load()) {
        if (reloadRequested.exchange(false)) {
            if (configManager.reloadIfChanged()) {
                LOG_INFO("bootstrap", "Configuration reload requested but runtime hot-reload not implemented for all services.");
            }
        } else {
            configManager.reloadIfChanged();
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    videoManager.stop();
    telemetryService.stop();
    publisher.stop();
    healthMonitor.stop();

    return EXIT_SUCCESS;
}
