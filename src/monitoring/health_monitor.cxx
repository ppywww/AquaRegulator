#include "monitoring/health_monitor.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "core/logger.hpp"

namespace monitoring {

HealthMonitor::HealthMonitor(std::string path, std::chrono::seconds interval)
    : filePath_(std::move(path))
    , interval_(interval) {
}

HealthMonitor::~HealthMonitor() {
    stop();
}

void HealthMonitor::start() {
    if (running_.exchange(true)) {
        return;
    }
    worker_ = std::thread(&HealthMonitor::writerLoop, this);
}

void HealthMonitor::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

void HealthMonitor::update(const std::string& component, bool healthy, const std::string& detail) {
    std::lock_guard<std::mutex> lk(mutex_);
    states_[component] = HealthState{healthy, detail, std::chrono::system_clock::now()};
}

void HealthMonitor::writerLoop() {
    while (running_) {
        flushToDisk();
        std::this_thread::sleep_for(interval_);
    }
    flushToDisk();
}

void HealthMonitor::flushToDisk() {
    std::map<std::string, HealthState> snapshot;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        snapshot = states_;
    }

    nlohmann::json json;
    for (const auto& [component, state] : snapshot) {
        auto time = std::chrono::system_clock::to_time_t(state.updatedAt);
        json[component] = {
            {"healthy", state.healthy},
            {"detail", state.detail},
            {"updatedAt", time}
        };
    }

    try {
        std::filesystem::create_directories(std::filesystem::path(filePath_).parent_path());
        std::ofstream out(filePath_, std::ios::out | std::ios::trunc);
        out << json.dump(4);
    } catch (const std::exception& ex) {
        LOG_ERROR("health_monitor", "Failed to persist health information: ", ex.what());
    }
}

} // namespace monitoring
