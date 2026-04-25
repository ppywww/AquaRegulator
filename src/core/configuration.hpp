#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace core {

struct DatabaseConfig {
    std::string host = "127.0.0.1";
    std::string user = "user";
    std::string password = "password";
    std::string schema = "testdb";
    uint16_t port = 3306;
    uint16_t readRecentLimit = 50;
    uint16_t retrySeconds = 5;
};

struct SensorConfig {
    std::string endpoint = "127.0.0.1";
    uint16_t port = 502;
    uint16_t retrySeconds = 5;
    uint16_t registers = 6;
};

struct PublisherConfig {
    std::string bindAddress = "0.0.0.0";
    uint16_t port = 5555;
    uint16_t workerThreads = 4;
    uint16_t maxConnections = 200;
};

struct VideoConfig {
    uint16_t port = 6000;
};

struct HealthConfig {
    std::string statusFile = "artifacts/health_status.json";
    uint16_t intervalSeconds = 5;
};

struct PipelineConfig {
    uint16_t realtimeIntervalSeconds = 5;
    uint16_t historicalIntervalSeconds = 30;
    uint16_t cacheSize = 120;
};

struct Configuration {
    DatabaseConfig database;
    SensorConfig sensor;
    PublisherConfig publisher;
    VideoConfig video;
    HealthConfig health;
    PipelineConfig pipeline;
};

class ConfigurationManager {
public:
    explicit ConfigurationManager(std::string path);

    const Configuration& get() const { return config_; }

    bool reloadIfChanged();

private:
    void loadFromDisk();
    static Configuration fromJson(const std::string& jsonText);
    static std::string defaultJson();

    Configuration config_;
    std::string path_;
    std::filesystem::file_time_type lastWriteTime_{};
};

} // namespace core
