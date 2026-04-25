#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "infrastructure/sensors/sensor_data.hpp"

class DeviceCommandRouter {
public:
    using DiagnosticProvider = std::function<nlohmann::json()>;
    using ReloadCallback = std::function<void()>;
    using ResponseCallback = std::function<void(const std::string&)>;

    DeviceCommandRouter(SensorGateway& gateway,
                        monitoring::HealthMonitor& monitor,
                        DiagnosticProvider diagnostics,
                        ReloadCallback reloadCallback)
        : sensorGateway_(gateway)
        , monitor_(monitor)
        , diagnosticsProvider_(std::move(diagnostics))
        , reloadCallback_(std::move(reloadCallback)) {}

    void feed(uint64_t connectionId, const std::string& chunk, const ResponseCallback& respond) {
        auto& buffer = buffers_[connectionId];
        buffer += chunk;

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            auto reply = parseLine(line);
            if (!reply.empty() && respond) {
                respond(reply);
            }
        }
    }

private:
    std::string parseLine(const std::string& line) {
        try {
            auto msg = nlohmann::json::parse(line);
            const std::string type = msg.value("type", "");
            if (type == "threshold") {
                handleThreshold(msg);
                return R"({"status":"ok","message":"threshold updated"})";
            } else if (type == "light_control") {
                handleLightControl(msg);
                return R"({"status":"ok","message":"light control updated"})";
            } else if (type == "mode_select") {
                handleModeSelect(msg);
                return R"({"status":"ok","message":"mode updated"})";
            } else if (type == "diagnostics") {
                return diagnosticsProvider_().dump();
            } else if (type == "config_reload") {
                if (reloadCallback_) {
                    reloadCallback_();
                }
                return R"({"status":"ok","message":"configuration reload requested"})";
            } else if (type == "write_register") {
                handleDirectWrite(msg);
                return R"({"status":"ok","message":"register write queued"})";
            }
            return R"({"status":"error","message":"unknown command"})";
        } catch (const std::exception& ex) {
            monitor_.update("command_router", false, ex.what());
            return R"({"status":"error","message":"invalid payload"})";
        }
    }

    void handleThreshold(const nlohmann::json& msg) {
        double soil = msg.value("soil", 0.0);
        double rain = msg.value("rain", 0.0);
        double temp = msg.value("temp", 0.0);
        double light = msg.value("light", 0.0);
        sensorGateway_.writeRegister(10, static_cast<uint16_t>(soil * 100));
        sensorGateway_.writeRegister(11, static_cast<uint16_t>(rain * 100));
        sensorGateway_.writeRegister(12, static_cast<uint16_t>(temp * 100));
        sensorGateway_.writeRegister(13, static_cast<uint16_t>(light * 100));
        monitor_.update("command_router", true, "threshold updated");
    }

    void handleLightControl(const nlohmann::json& msg) {
        double light = msg.value("light", 0.0);
        sensorGateway_.writeRegister(14, static_cast<uint16_t>(light * 100));
        monitor_.update("command_router", true, "light control updated");
    }

    void handleModeSelect(const nlohmann::json& msg) {
        int mode = msg.value("mode", 0);
        sensorGateway_.writeRegister(15, static_cast<uint16_t>(mode));
        monitor_.update("command_router", true, "mode updated");
    }

    void handleDirectWrite(const nlohmann::json& msg) {
        int address = msg.value("address", -1);
        int value = msg.value("value", 0);
        if (address >= 0) {
            sensorGateway_.writeRegister(static_cast<uint16_t>(address), static_cast<uint16_t>(value));
        }
    }

    SensorGateway& sensorGateway_;
    monitoring::HealthMonitor& monitor_;
    DiagnosticProvider diagnosticsProvider_;
    ReloadCallback reloadCallback_;
    std::unordered_map<uint64_t, std::string> buffers_;
};
