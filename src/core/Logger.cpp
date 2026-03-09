#include "sure_smartie/core/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>

namespace sure_smartie::core {
namespace {

std::mutex g_log_mutex;
LogLevel g_log_level = LogLevel::info;

std::string toLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char symbol) {
    return static_cast<char>(std::tolower(symbol));
  });
  return value;
}

std::string_view levelName(LogLevel level) {
  switch (level) {
    case LogLevel::trace:
      return "trace";
    case LogLevel::debug:
      return "debug";
    case LogLevel::info:
      return "info";
    case LogLevel::warn:
      return "warn";
    case LogLevel::error:
      return "error";
  }

  return "info";
}

std::string quoteValue(std::string_view value) {
  std::string output;
  output.reserve(value.size() + 2);
  output.push_back('"');
  for (char symbol : value) {
    if (symbol == '"' || symbol == '\\') {
      output.push_back('\\');
    }
    if (symbol == '\n' || symbol == '\r') {
      output.push_back(' ');
      continue;
    }
    output.push_back(symbol);
  }
  output.push_back('"');
  return output;
}

}  // namespace

LogLevel parseLogLevel(std::string_view value) {
  const auto lowered = toLower(std::string{value});

  if (lowered == "trace") {
    return LogLevel::trace;
  }
  if (lowered == "debug") {
    return LogLevel::debug;
  }
  if (lowered == "info") {
    return LogLevel::info;
  }
  if (lowered == "warn" || lowered == "warning") {
    return LogLevel::warn;
  }
  if (lowered == "error") {
    return LogLevel::error;
  }

  throw std::invalid_argument("Unknown log level: " + std::string{value});
}

void setLogLevel(LogLevel level) { g_log_level = level; }

LogLevel currentLogLevel() { return g_log_level; }

void logMessage(
    LogLevel level,
    std::string_view component,
    std::string_view message,
    std::initializer_list<std::pair<std::string_view, std::string>> fields) {
  if (static_cast<int>(level) < static_cast<int>(g_log_level)) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_log_mutex);
  std::cerr << "level=" << levelName(level)
            << " component=" << component
            << " msg=" << quoteValue(message);
  for (const auto& [key, value] : fields) {
    std::cerr << ' ' << key << '=' << quoteValue(value);
  }
  std::cerr << '\n';
}

}  // namespace sure_smartie::core
