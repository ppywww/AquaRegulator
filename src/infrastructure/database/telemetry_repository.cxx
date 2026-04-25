#include "infrastructure/database/telemetry_repository.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "core/logger.hpp"

namespace infrastructure::database {

bool TelemetryRepository::initialize(const core::DatabaseConfig& cfg) {
    config_ = cfg;
    if (!client_.initialize()) {
        return false;
    }
    return client_.connect(cfg);
}

void TelemetryRepository::refreshConnection() {
    if (client_.isConnected() && client_.ping()) {
        return;
    }
    LOG_WARN("telemetry_repo", "Refreshing MariaDB connection...");
    client_.disconnect();
    client_.initialize();
    client_.connect(config_);
}

std::vector<domain::TelemetryReading> TelemetryRepository::loadEnvironmental(std::size_t limit) {//加载环境数据
    //从数据库中查询环境数据
    refreshConnection();

    std::ostringstream oss;
    oss << "SELECT time, temperature, humidity, light "
        << "FROM environmental_conditions "
        << "ORDER BY time DESC LIMIT " << limit;

    if (!client_.execute(oss.str())) {
        return {};
    }

    MYSQL_RES* res = client_.storeResult();
    if (res == nullptr) {
        LOG_ERROR("telemetry_repo", "mysql_store_result() returned null");
        return {};
    }

    std::vector<domain::TelemetryReading> readings;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        readings.emplace_back(buildEnvReading(row));
    }
    mysql_free_result(res);

    std::reverse(readings.begin(), readings.end());
    return readings;
}

std::vector<domain::TelemetryReading> TelemetryRepository::loadSoilAndAir(std::size_t limit) {//加载土壤和空气数据
    //从数据库中查询土壤和空气数据
    refreshConnection();

    std::ostringstream oss;
    oss << "SELECT time, soil, gas, raindrop "
        << "FROM soil_and_air_quality "
        << "ORDER BY time DESC LIMIT " << limit;

    if (!client_.execute(oss.str())) {
        return {};
    }

    MYSQL_RES* res = client_.storeResult();
    if (res == nullptr) {
        LOG_ERROR("telemetry_repo", "mysql_store_result() returned null");
        return {};
    }

    std::vector<domain::TelemetryReading> readings;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        readings.emplace_back(buildSoilReading(row));
    }
    mysql_free_result(res);

    std::reverse(readings.begin(), readings.end());
    return readings;
}

domain::TelemetryReading TelemetryRepository::buildEnvReading(MYSQL_ROW row) const {//构建环境数据
    domain::TelemetryReading reading;
    reading.label = "Historical_ENV";
    reading.timestamp = row[0] ? row[0] : "N/A";
    reading.temperature = row[1] ? std::stod(row[1]) : 0.0;
    reading.humidity = row[2] ? std::stod(row[2]) : 0.0;
    reading.light = row[3] ? std::stod(row[3]) : 0.0;
    return reading;
}

domain::TelemetryReading TelemetryRepository::buildSoilReading(MYSQL_ROW row) const {//构建土壤数据
    domain::TelemetryReading reading;
    reading.label = "Historical_Soil";
    reading.timestamp = row[0] ? row[0] : "N/A";
    reading.soil = row[1] ? std::stod(row[1]) : 0.0;
    reading.gas = row[2] ? std::stod(row[2]) : 0.0;
    reading.raindrop = row[3] ? std::stod(row[3]) : 0.0;
    return reading;
}


std::string TelemetryRepository::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_s(&tm_now, &time_t_now);
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool TelemetryRepository::saveEnvironmental(const domain::TelemetryReading& reading) {
    refreshConnection();
    
    std::string timestamp = reading.timestamp.empty() ? getCurrentTimestamp() : reading.timestamp;
    std::ostringstream oss;
    oss << "INSERT INTO environmental_conditions (time, temperature, humidity, light) "
        << "VALUES ('" << timestamp << "', "
        << reading.temperature << ", "
        << reading.humidity << ", "
        << reading.light << ")";
    
    if (!client_.execute(oss.str())) {
        LOG_ERROR("telemetry_repo", "Failed to save environmental data: ", client_.getError());
        return false;
    }
    return true;
}

bool TelemetryRepository::saveSoilAndAir(const domain::TelemetryReading& reading) {
    refreshConnection();
    
    std::string timestamp = reading.timestamp.empty() ? getCurrentTimestamp() : reading.timestamp;
    std::ostringstream oss;
    oss << "INSERT INTO soil_and_air_quality (time, soil, gas, raindrop) "
        << "VALUES ('" << timestamp << "', "
        << reading.soil << ", "
        << reading.gas << ", "
        << reading.raindrop << ")";
    
    if (!client_.execute(oss.str())) {
        LOG_ERROR("telemetry_repo", "Failed to save soil and air data: ", client_.getError());
        return false;
    }
    return true;
}

bool TelemetryRepository::saveRealtime(const domain::TelemetryReading& reading) {
    bool envResult = saveEnvironmental(reading);
    bool soilResult = saveSoilAndAir(reading);
    return envResult && soilResult;
}

} // namespace infrastructure::database
