#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace forgedb {

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
    explicit Logger(std::string filePath = {});
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    void log(LogLevel level, std::string_view message);
    void debug(std::string_view message) { log(LogLevel::Debug, message); }
    void info(std::string_view message) { log(LogLevel::Info, message); }
    void warning(std::string_view message) { log(LogLevel::Warning, message); }
    void error(std::string_view message) { log(LogLevel::Error, message); }
    void flush();
private:
    std::mutex mutex_;
    std::ofstream file_;
};
} // namespace forgedb
