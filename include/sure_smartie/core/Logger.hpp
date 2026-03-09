#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace sure_smartie::core {

enum class LogLevel {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  error = 4,
};

LogLevel parseLogLevel(std::string_view value);
void setLogLevel(LogLevel level);
LogLevel currentLogLevel();

void logMessage(
    LogLevel level,
    std::string_view component,
    std::string_view message,
    std::initializer_list<std::pair<std::string_view, std::string>> fields = {});

}  // namespace sure_smartie::core
