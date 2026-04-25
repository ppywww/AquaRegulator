#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace monitoring {

struct HealthState {
    bool healthy{false};
    std::string detail;
    std::chrono::system_clock::time_point updatedAt;
};

class HealthMonitor {
public:
    HealthMonitor(std::string path, std::chrono::seconds interval);
    ~HealthMonitor();

    void start();
    void stop();

    void update(const std::string& component, bool healthy, const std::string& detail);

private:
    void writerLoop();
    void flushToDisk();

    std::string filePath_;
    std::chrono::seconds interval_;

    std::map<std::string, HealthState> states_;
    std::mutex mutex_;

    std::thread worker_;
    std::atomic<bool> running_{false};
};

} // namespace monitoring
