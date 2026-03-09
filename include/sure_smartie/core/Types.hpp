#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sure_smartie::core {

using MetricMap = std::unordered_map<std::string, std::string>;
using Frame = std::vector<std::string>;

struct DisplayGeometry {
  std::size_t cols{20};
  std::size_t rows{4};
};

struct DisplayConfig {
  std::string type{"sure"};
  std::size_t cols{20};
  std::size_t rows{4};
  bool backlight{true};
  std::uint8_t contrast{0x80};
  std::uint8_t brightness{0xC0};
};

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
};

}  // namespace sure_smartie::core
