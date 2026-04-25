#include "core/configuration.hpp"

#include <fstream>
#include <filesystem>
#include <sstream>
#include <nlohmann/json.hpp>

#include "core/logger.hpp"

namespace core {
namespace {

Configuration buildDefaultConfig() {
    Configuration cfg;
    return cfg;
}

} // namespace

ConfigurationManager::ConfigurationManager(std::string path)
    : config_(buildDefaultConfig())
    , path_(std::move(path)) {
    loadFromDisk();
}

bool ConfigurationManager::reloadIfChanged() {
    std::error_code ec;
    auto current = std::filesystem::last_write_time(path_, ec);
    if (ec) {
        return false;
    }
    if (current != lastWriteTime_) {
        loadFromDisk();
        lastWriteTime_ = current;
        return true;
    }
    return false;
}

void ConfigurationManager::loadFromDisk() {
    std::ifstream in(path_);
    if (!in.good()) {
        std::filesystem::create_directories(std::filesystem::path(path_).parent_path());
        std::ofstream out(path_);
        out << defaultJson();
        out.close();
        LOG_WARN("config", "Configuration file missing. A default template was created at ", path_);
        config_ = buildDefaultConfig();
        return;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    config_ = fromJson(buffer.str());

    std::error_code ec;
    lastWriteTime_ = std::filesystem::last_write_time(path_, ec);
}

Configuration ConfigurationManager::fromJson(const std::string& jsonText) {
    Configuration cfg = buildDefaultConfig();
    try {
        auto json = nlohmann::json::parse(jsonText);

        if (auto it = json.find("database"); it != json.end()) {
            cfg.database.host = it->value("host", cfg.database.host);
            cfg.database.user = it->value("user", cfg.database.user);
            cfg.database.password = it->value("password", cfg.database.password);
            cfg.database.schema = it->value("schema", cfg.database.schema);
            cfg.database.port = it->value("port", cfg.database.port);
            cfg.database.readRecentLimit = it->value("recentLimit", cfg.database.readRecentLimit);
            cfg.database.retrySeconds = it->value("retrySeconds", cfg.database.retrySeconds);
        }

        if (auto it = json.find("sensor"); it != json.end()) {
            cfg.sensor.endpoint = it->value("endpoint", cfg.sensor.endpoint);
            cfg.sensor.port = it->value("port", cfg.sensor.port);
            cfg.sensor.retrySeconds = it->value("retrySeconds", cfg.sensor.retrySeconds);
            cfg.sensor.registers = it->value("registers", cfg.sensor.registers);
        }

        if (auto it = json.find("publisher"); it != json.end()) {
            cfg.publisher.bindAddress = it->value("bindAddress", cfg.publisher.bindAddress);
            cfg.publisher.port = it->value("port", cfg.publisher.port);
            cfg.publisher.workerThreads = it->value("workerThreads", cfg.publisher.workerThreads);
            cfg.publisher.maxConnections = it->value("maxConnections", cfg.publisher.maxConnections);
        }

        if (auto it = json.find("video"); it != json.end()) {
            cfg.video.port = it->value("port", cfg.video.port);
        }

        if (auto it = json.find("health"); it != json.end()) {
            cfg.health.statusFile = it->value("statusFile", cfg.health.statusFile);
            cfg.health.intervalSeconds = it->value("intervalSeconds", cfg.health.intervalSeconds);
        }

        if (auto it = json.find("pipeline"); it != json.end()) {
            cfg.pipeline.realtimeIntervalSeconds = it->value("realtimeSeconds", cfg.pipeline.realtimeIntervalSeconds);
            cfg.pipeline.historicalIntervalSeconds = it->value("historicalSeconds", cfg.pipeline.historicalIntervalSeconds);
            cfg.pipeline.cacheSize = it->value("cacheSize", cfg.pipeline.cacheSize);
        }

    } catch (const std::exception& ex) {
        LOG_ERROR("config", "Failed to parse configuration. Using defaults. Error: ", ex.what());
    }
    return cfg;
}

std::string ConfigurationManager::defaultJson() {
    nlohmann::json json{
        {"database",
         {{"host", "192.168.125.128"},
          {"user", "devuser"},
          {"password", "123456"},
          {"schema", "testdb"},
          {"port", 3306},
          {"recentLimit", 50},
          {"retrySeconds", 5}}},
        {"sensor",
         {{"endpoint", "192.168.125.128"},
          {"port", 502},
          {"retrySeconds", 5},
          {"registers", 6}}},
        {"publisher",
         {{"bindAddress", "0.0.0.0"},
          {"port", 5555},
          {"workerThreads", 4},
          {"maxConnections", 200}}},
        {"video",
         {{"port", 6000}}},
        {"health",
         {{"statusFile", "artifacts/health_status.json"},
          {"intervalSeconds", 10}}},
        {"pipeline",
         {{"realtimeSeconds", 5},
          {"historicalSeconds", 60},
          {"cacheSize", 120}}}
    };

    return json.dump(4);
}

} // namespace core
