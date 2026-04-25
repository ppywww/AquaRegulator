#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace core {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Critical
};

class Logger {
public:
    static Logger& instance();

    void configure(LogLevel level, const std::string& filePath = "", bool useConsole = true);

    template <typename... Args>
    void log(LogLevel level, std::string_view component, Args&&... args) {
        if (level < minLevel_.load()) {
            return;
        }

        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        write(level, component, oss.str());
    }

private:
    Logger() = default;
    ~Logger() = default;

    void write(LogLevel level, std::string_view component, const std::string& message);
    std::string levelToString(LogLevel level) const;

    std::mutex mutex_;
    std::atomic<LogLevel> minLevel_{LogLevel::Info};
    std::ofstream fileStream_;
    bool consoleEnabled_{true};
    bool fileEnabled_{false};
};

} // namespace core

#define LOG_TRACE(component, ...) ::core::Logger::instance().log(::core::LogLevel::Trace, component, __VA_ARGS__)
#define LOG_DEBUG(component, ...) ::core::Logger::instance().log(::core::LogLevel::Debug, component, __VA_ARGS__)
#define LOG_INFO(component, ...)  ::core::Logger::instance().log(::core::LogLevel::Info, component, __VA_ARGS__)
#define LOG_WARN(component, ...)  ::core::Logger::instance().log(::core::LogLevel::Warn, component, __VA_ARGS__)
#define LOG_ERROR(component, ...) ::core::Logger::instance().log(::core::LogLevel::Error, component, __VA_ARGS__)
#define LOG_CRITICAL(component, ...) ::core::Logger::instance().log(::core::LogLevel::Critical, component, __VA_ARGS__)
