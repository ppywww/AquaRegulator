#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace domain {

enum class TelemetryChannel {
    Realtime,
    HistoricalEnvironment,
    HistoricalSoil
};

struct TelemetryChannelHash {
    std::size_t operator()(TelemetryChannel channel) const noexcept {
        return static_cast<std::size_t>(channel);
    }
};

struct TelemetryReading {
    std::string label{"Realtime"};
    std::string timestamp;
    double temperature{0.0};
    double humidity{0.0};
    double light{0.0};
    double soil{0.0};
    double gas{0.0};
    double raindrop{0.0};
};

struct TelemetryFrame {
    TelemetryChannel channel{TelemetryChannel::Realtime};
    std::vector<TelemetryReading> readings;
    bool snapshot{true};
    std::string correlationId;
};

inline std::string channelName(TelemetryChannel channel) {
    switch (channel) {
    case TelemetryChannel::Realtime: return "realtime";
    case TelemetryChannel::HistoricalEnvironment: return "historical_env";
    case TelemetryChannel::HistoricalSoil: return "historical_soil";
    default: return "unknown";
    }
}

inline nlohmann::json toJson(const TelemetryReading& reading) {
    return nlohmann::json{
        {"label", reading.label},
        {"timestamp", reading.timestamp},
        {"temperature", reading.temperature},
        {"humidity", reading.humidity},
        {"light", reading.light},
        {"soil", reading.soil},
        {"gas", reading.gas},
        {"raindrop", reading.raindrop}
    };
}

inline nlohmann::json toJson(const TelemetryFrame& frame) {
    nlohmann::json json;
    json["channel"] = channelName(frame.channel);
    json["snapshot"] = frame.snapshot;
    json["correlationId"] = frame.correlationId;
    json["readings"] = nlohmann::json::array();
    for (const auto& reading : frame.readings) {
        json["readings"].push_back(toJson(reading));
    }
    return json;
}

} // namespace domain
