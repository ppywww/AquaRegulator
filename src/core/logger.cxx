#include "core/logger.hpp"

#include <ctime>
#include <filesystem>
#include <iostream>

namespace core {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::configure(LogLevel level, const std::string& filePath, bool useConsole) {
    minLevel_.store(level);
    consoleEnabled_ = useConsole;

    if (!filePath.empty()) {
        std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
        fileStream_.open(filePath, std::ios::out | std::ios::app);
        fileEnabled_ = fileStream_.is_open();
    }
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Critical: return "CRITICAL";
    default: return "UNKNOWN";
    }
}

void Logger::write(LogLevel level, std::string_view component, const std::string& message) {
    std::lock_guard<std::mutex> lk(mutex_);

    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream prefix;
    prefix << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
           << " [" << levelToString(level) << "]"
           << " [" << component << "] ";

    if (consoleEnabled_) {
        std::cout << prefix.str() << message << std::endl;
    }
    if (fileEnabled_) {
        fileStream_ << prefix.str() << message << std::endl;
    }
}

} // namespace core
