#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "core/configuration.hpp"
#include "core/logger.hpp"
#include "domain/telemetry_models.hpp"
#include "modbus_tcp.hpp"
#include "monitoring/health_monitor.hpp"

class SensorGateway {//传感器网关：负责与传感器进行通信，读取传感器数据并将其转换为域模型
public:
    SensorGateway(core::SensorConfig config, monitoring::HealthMonitor& monitor)
        : config_(std::move(config))//使用移动构造函数移动配置对象，避免复制，提高效率，同时避免配置对象被重复使用
        , monitor_(monitor) {
    }

    ~SensorGateway() {
        disconnect();
    }

    std::optional<domain::TelemetryReading> readRealtime() {//读取实时数据

        std::lock_guard<std::mutex> lk(mutex_);//防止多个线程同时读取传感器数据
        if (!ensureConnection()) {
            return std::nullopt;
        }

        std::vector<uint16_t> registers(config_.registers, 0);//寄存器值数组
        try {
            modbus_->readRegisters(0, config_.registers, registers.data());//读取寄存器
        } catch (const std::exception& ex) {
            handleFailure(std::string("readRegisters failed: ") + ex.what());
            return std::nullopt;
        }
        //把寄存器值转换为浮点数，单位为摄氏度、湿度、光照强度
        domain::TelemetryReading reading;
        reading.label = "Realtime";
        reading.timestamp = currentTimestamp();
        if (registers.size() >= 6) {
            reading.soil = registers[0] / 100.0;
            reading.gas = registers[1] / 100.0;
            reading.raindrop = registers[2] / 100.0;
            reading.temperature = registers[3] / 100.0;
            reading.humidity = registers[4] / 100.0;
            reading.light = registers[5] / 100.0;
        }

        monitor_.update("sensor_gateway", true, "Realtime sample collected");
        return reading;
    }

    void writeRegister(uint16_t address, uint16_t value) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!ensureConnection()) {
            return;
        }
        try {
            modbus_->writeRegister(address, value);//写入寄存器
            monitor_.update("sensor_gateway", true, "Register write successful");
        } catch (const std::exception& ex) {
            handleFailure(std::string("writeRegister failed: ") + ex.what());
        }
    }

private:
    bool ensureConnection() {//服务器主动连接传感器，如果连接失败，重试config_.retrySeconds秒
        auto now = std::chrono::steady_clock::now();
        if (modbus_) {//如果modbus_指针不为空，说明已连接传感器，直接返回true
            return true;
        }
        
        


        //如果上次尝试连接失败，且距离上次尝试时间小于 重试间隔 = 5· 秒，直接返回false
        if (now - lastAttempt_ < std::chrono::seconds(config_.retrySeconds)) {
            return false;
        }
        lastAttempt_ = now;

        //尝试连接传感器，如果连接成功，返回true
        try {
            modbus_ = std::make_unique<ModbusTCP>(config_.endpoint.c_str(), config_.port);
            modbus_->connect();
            monitor_.update("sensor_gateway", true, "Modbus connected");
            LOG_INFO("sensor_gateway", "Connected to Modbus sensor at ", config_.endpoint, ":", config_.port);
            return true;
        } catch (const std::exception& ex) {//如果连接失败，重置modbus_指针，返回false
            modbus_.reset();
            handleFailure(std::string("Connection error: ") + ex.what());
            return false;
        }
    }

    void disconnect() {
        std::lock_guard<std::mutex> lk(mutex_);
        modbus_.reset();
    }

    void handleFailure(const std::string& reason) {
        LOG_WARN("sensor_gateway", reason);
        monitor_.update("sensor_gateway", false, reason);

        // 重置modbus_指针，准备下一次重试
        modbus_.reset();
    }

    std::string currentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    core::SensorConfig config_;
    monitoring::HealthMonitor& monitor_;
    std::unique_ptr<ModbusTCP> modbus_;
    std::mutex mutex_;
    std::chrono::steady_clock::time_point lastAttempt_{};
};
