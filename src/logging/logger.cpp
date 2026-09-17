#include "forgedb/logging/logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace forgedb {
Logger::Logger(std::string filePath) { if (!filePath.empty()) file_.open(filePath, std::ios::app); }
Logger::~Logger() { flush(); }
void Logger::log(LogLevel level, std::string_view message) {
    std::lock_guard lock(mutex_);
    const char* label = "INFO";
    if (level == LogLevel::Debug) label = "DEBUG";
    else if (level == LogLevel::Warning) label = "WARN";
    else if (level == LogLevel::Error) label = "ERROR";
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    std::ostringstream line;
    line << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [" << label << "] " << message << '\n';
    const auto text = line.str();
    if (file_.is_open()) file_ << text; else std::clog << text;
}
void Logger::flush() { std::lock_guard lock(mutex_); if (file_.is_open()) file_.flush(); }
} // namespace forgedb
