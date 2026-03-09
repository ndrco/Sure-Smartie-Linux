#include "sure_smartie/core/Config.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace sure_smartie::core {
namespace {

using json = nlohmann::json;

std::chrono::milliseconds normalizedInterval(int value_ms) {
  return std::chrono::milliseconds(std::max(value_ms, 100));
}

std::uint8_t clampByte(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 1, 254));
}

ScreenDefinition defaultScreen() {
  return ScreenDefinition{
      .name = "overview",
      .interval = std::chrono::milliseconds{2000},
      .lines =
          {
              "CPU {cpu.load}% {cpu.temp}C",
              "RAM {ram.percent}% {ram.used_gb}/{ram.total_gb}",
              "IP  {net.ip}",
              "{system.time} {system.hostname}",
          },
  };
}

}  // namespace

AppConfig ConfigLoader::loadFromFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Unable to open config: " + path.string());
  }

  json document = json::parse(input, nullptr, true, true);
  AppConfig config;

  if (document.contains("device")) {
    config.device = document.at("device").get<std::string>();
  }

  if (document.contains("baudrate")) {
    config.baudrate = document.at("baudrate").get<int>();
  }

  if (document.contains("refresh_ms")) {
    config.refresh_interval =
        normalizedInterval(document.at("refresh_ms").get<int>());
  }

  if (document.contains("display")) {
    const auto& display = document.at("display");
    if (display.contains("type")) {
      config.display.type = display.at("type").get<std::string>();
    }
    if (display.contains("cols")) {
      config.display.cols = std::max<std::size_t>(display.at("cols").get<int>(), 1);
    }
    if (display.contains("rows")) {
      config.display.rows = std::max<std::size_t>(display.at("rows").get<int>(), 1);
    }
    if (display.contains("backlight")) {
      config.display.backlight = display.at("backlight").get<bool>();
    }
    if (display.contains("contrast")) {
      config.display.contrast = clampByte(display.at("contrast").get<int>());
    }
    if (display.contains("brightness")) {
      config.display.brightness = clampByte(display.at("brightness").get<int>());
    }
  }

  if (document.contains("providers")) {
    config.providers = document.at("providers").get<std::vector<std::string>>();
  }

  config.screens.clear();
  if (document.contains("screens")) {
    for (const auto& screen_json : document.at("screens")) {
      ScreenDefinition screen;
      screen.name = screen_json.value("name", "screen");
      screen.interval =
          normalizedInterval(screen_json.value("interval_ms", 2000));
      if (screen_json.contains("lines")) {
        screen.lines =
            screen_json.at("lines").get<std::vector<std::string>>();
      }
      config.screens.push_back(std::move(screen));
    }
  }

  if (config.screens.empty()) {
    config.screens.push_back(defaultScreen());
  }

  return config;
}

}  // namespace sure_smartie::core
