#pragma once

#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "domain/telemetry_models.hpp"

namespace infrastructure::cache {

class TelemetryCache {
public:
    explicit TelemetryCache(std::size_t capacityPerChannel)
        : capacity_(capacityPerChannel) {}

    void store(domain::TelemetryChannel channel, const domain::TelemetryReading& reading) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& buffer = cache_[channel];
        buffer.push_back(reading);
        if (buffer.size() > capacity_) {
            buffer.pop_front();
        }
    }

    std::vector<domain::TelemetryReading> snapshot(domain::TelemetryChannel channel) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<domain::TelemetryReading> copy;
        if (auto it = cache_.find(channel); it != cache_.end()) {
            copy.assign(it->second.begin(), it->second.end());
        }
        return copy;
    }

    std::vector<domain::TelemetryReading> snapshotAll() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<domain::TelemetryReading> copy;
        for (const auto& [_, buffer] : cache_) {
            copy.insert(copy.end(), buffer.begin(), buffer.end());
        }
        return copy;
    }

private:
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::unordered_map<domain::TelemetryChannel, std::deque<domain::TelemetryReading>, domain::TelemetryChannelHash> cache_;
};

} // namespace infrastructure::cache
