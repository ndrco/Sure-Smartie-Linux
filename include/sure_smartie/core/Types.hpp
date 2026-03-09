#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sure_smartie::core {

using MetricMap = std::unordered_map<std::string, std::string>;
using Frame = std::vector<std::string>;

enum class DiagnosticSeverity {
  info,
  warning,
  error,
};

struct Diagnostic {
  DiagnosticSeverity severity{DiagnosticSeverity::info};
  std::string field_path;
  std::string message;
};

struct DisplayGeometry {
  std::size_t cols{20};
  std::size_t rows{4};
};

struct DisplayConfig {
  std::string type{"sure"};
  int cols{20};
  int rows{4};
  bool backlight{true};
  int contrast{0x80};
  int brightness{0xC0};
};

inline DisplayGeometry normalizedGeometry(const DisplayConfig& config) {
  return DisplayGeometry{
      .cols = static_cast<std::size_t>(std::max(config.cols, 1)),
      .rows = static_cast<std::size_t>(std::max(config.rows, 1)),
  };
}

struct ScreenDefinition {
  std::string name;
  std::chrono::milliseconds interval{2000};
  std::vector<std::string> lines;
};

struct AppConfig {
  std::string device{"/dev/ttyUSB1"};
  int baudrate{9600};
  std::chrono::milliseconds refresh_interval{1000};
  DisplayConfig display;
  std::vector<std::string> providers{"cpu", "gpu", "ram", "system", "network"};
  std::vector<std::string> plugin_paths;
  std::vector<ScreenDefinition> screens;
};

struct RuntimeOptions {
  bool once{false};
  bool force_stdout_display{false};
  bool validate_config_only{false};
};

}  // namespace sure_smartie::core
