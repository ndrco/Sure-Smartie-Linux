#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sure_smartie::core {

using MetricMap = std::unordered_map<std::string, std::string>;
using Frame = std::vector<std::string>;
using GlyphPattern = std::array<int, 8>;

inline constexpr std::size_t kGlyphSlotCount = 8;
inline constexpr std::size_t kGlyphWidth = 5;
inline constexpr std::size_t kGlyphHeight = 8;

inline constexpr char kGlyphBar1 = '\x01';
inline constexpr char kGlyphBar2 = '\x02';
inline constexpr char kGlyphBar3 = '\x03';
inline constexpr char kGlyphBar4 = '\x04';
inline constexpr char kGlyphBar5 = '\x05';
inline constexpr char kGlyphBarBase = '\x06';

struct CustomGlyphDefinition {
  std::string name;
  GlyphPattern pattern{};
};

struct ActiveGlyphSlot {
  bool active{false};
  std::string name;
  GlyphPattern pattern{};
};

using GlyphSlotBank = std::array<ActiveGlyphSlot, kGlyphSlotCount>;

struct RenderedFrame {
  Frame frame;
  GlyphSlotBank glyphs;
};

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

struct CpuFanConfig {
  std::string rpm_path;
  int max_rpm{0};
};

struct AppConfig {
  std::string device{"/dev/ttyUSB1"};
  int baudrate{9600};
  std::chrono::milliseconds refresh_interval{1000};
  bool auto_screen_rotation{true};
  DisplayConfig display;
  CpuFanConfig cpu_fan;
  std::vector<std::string> providers{"cpu", "gpu", "ram", "system", "network"};
  std::vector<std::string> plugin_paths;
  std::vector<CustomGlyphDefinition> custom_glyphs;
  std::vector<ScreenDefinition> screens;
};

struct RuntimeOptions {
  bool once{false};
  bool force_stdout_display{false};
  bool validate_config_only{false};
  bool auto_screen_rotation{true};
  std::optional<std::string> initial_screen_selector;
  std::filesystem::path screen_control_socket_path;
};

}  // namespace sure_smartie::core
