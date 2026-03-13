#include "sure_smartie/core/Config.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace sure_smartie::core {
namespace {

using json = nlohmann::json;

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
        std::chrono::milliseconds(document.at("refresh_ms").get<int>());
  }

  if (document.contains("display")) {
    const auto& display = document.at("display");
    if (display.contains("type")) {
      config.display.type = display.at("type").get<std::string>();
    }
    if (display.contains("cols")) {
      config.display.cols = display.at("cols").get<int>();
    }
    if (display.contains("rows")) {
      config.display.rows = display.at("rows").get<int>();
    }
    if (display.contains("backlight")) {
      config.display.backlight = display.at("backlight").get<bool>();
    }
    if (display.contains("contrast")) {
      config.display.contrast = display.at("contrast").get<int>();
    }
    if (display.contains("brightness")) {
      config.display.brightness = display.at("brightness").get<int>();
    }
  }

  if (document.contains("cpu_fan")) {
    const auto& cpu_fan = document.at("cpu_fan");
    if (cpu_fan.contains("rpm_path")) {
      config.cpu_fan.rpm_path = cpu_fan.at("rpm_path").get<std::string>();
    }
    if (cpu_fan.contains("max_rpm")) {
      config.cpu_fan.max_rpm = cpu_fan.at("max_rpm").get<int>();
    }
  }

  if (document.contains("providers")) {
    config.providers = document.at("providers").get<std::vector<std::string>>();
  }

  if (document.contains("plugin_paths")) {
    config.plugin_paths =
        document.at("plugin_paths").get<std::vector<std::string>>();
  }

  config.custom_glyphs.clear();
  if (document.contains("custom_glyphs")) {
    for (const auto& glyph_json : document.at("custom_glyphs")) {
      CustomGlyphDefinition glyph;
      glyph.name = glyph_json.value("name", "");
      if (glyph_json.contains("rows")) {
        const auto& rows = glyph_json.at("rows");
        const auto limit =
            std::min<std::size_t>(rows.size(), glyph.pattern.size());
        for (std::size_t index = 0; index < limit; ++index) {
          glyph.pattern[index] = rows.at(index).get<int>();
        }
      }
      config.custom_glyphs.push_back(std::move(glyph));
    }
  }

  config.screens.clear();
  if (document.contains("screens")) {
    for (const auto& screen_json : document.at("screens")) {
      ScreenDefinition screen;
      screen.name = screen_json.value("name", "screen");
      screen.interval = std::chrono::milliseconds(
          screen_json.value("interval_ms", 2000));
      if (screen_json.contains("lines")) {
        screen.lines =
            screen_json.at("lines").get<std::vector<std::string>>();
      }
      config.screens.push_back(std::move(screen));
    }
  }

  return config;
}

}  // namespace sure_smartie::core
