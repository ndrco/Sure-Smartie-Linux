#include "sure_smartie/core/ConfigSerializer.hpp"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace sure_smartie::core {
namespace {

nlohmann::json toJson(const AppConfig& config) {
  nlohmann::json document;
  document["device"] = config.device;
  document["baudrate"] = config.baudrate;
  document["refresh_ms"] = config.refresh_interval.count();
  document["display"] = {
      {"type", config.display.type},
      {"cols", config.display.cols},
      {"rows", config.display.rows},
      {"backlight", config.display.backlight},
      {"contrast", config.display.contrast},
      {"brightness", config.display.brightness},
  };
  document["cpu_fan"] = {
      {"rpm_path", config.cpu_fan.rpm_path},
      {"max_rpm", config.cpu_fan.max_rpm},
  };
  document["providers"] = config.providers;
  document["plugin_paths"] = config.plugin_paths;
  document["custom_glyphs"] = nlohmann::json::array();

  for (const auto& glyph : config.custom_glyphs) {
    document["custom_glyphs"].push_back({
        {"name", glyph.name},
        {"rows", glyph.pattern},
    });
  }

  document["screens"] = nlohmann::json::array();
  for (const auto& screen : config.screens) {
    document["screens"].push_back({
        {"name", screen.name},
        {"interval_ms", screen.interval.count()},
        {"lines", screen.lines},
    });
  }

  return document;
}

}  // namespace

std::string ConfigSerializer::serialize(const AppConfig& config) {
  return toJson(config).dump(2) + '\n';
}

void ConfigSerializer::saveToFile(const AppConfig& config,
                                  const std::filesystem::path& path) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("Unable to open config for writing: " + path.string());
  }

  output << serialize(config);
  if (!output) {
    throw std::runtime_error("Unable to write config: " + path.string());
  }
}

}  // namespace sure_smartie::core
