#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

#include "sure_smartie/core/Config.hpp"
#include "sure_smartie/core/ConfigSerializer.hpp"
#include "sure_smartie/core/ConfigValidator.hpp"
#include "sure_smartie/core/PreviewFrameRenderer.hpp"
#include "sure_smartie/core/Types.hpp"
#include "sure_smartie/engine/TemplateEngine.hpp"
#include "sure_smartie/providers/CpuProvider.hpp"
#include "sure_smartie/providers/GpuProvider.hpp"

namespace {

bool containsDiagnostic(const std::vector<sure_smartie::core::Diagnostic>& diagnostics,
                        sure_smartie::core::DiagnosticSeverity severity,
                        std::string_view field_path_fragment) {
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.severity == severity &&
        diagnostic.field_path.find(field_path_fragment) != std::string::npos) {
      return true;
    }
  }

  return false;
}

}  // namespace

int main() {
  using namespace std::chrono_literals;
  using sure_smartie::core::AppConfig;
  using sure_smartie::core::ConfigLoader;
  using sure_smartie::core::ConfigSerializer;
  using sure_smartie::core::ConfigValidator;
  using sure_smartie::core::DiagnosticSeverity;
  using sure_smartie::core::MetricMap;
  using sure_smartie::core::PreviewFrameRenderer;
  using sure_smartie::core::ScreenDefinition;

  AppConfig config;
  config.device = "/dev/null";
  config.baudrate = 115200;
  config.refresh_interval = 1500ms;
  config.display.type = "stdout";
  config.display.cols = 20;
  config.display.rows = 4;
  config.display.backlight = false;
  config.display.contrast = 120;
  config.display.brightness = 180;
  config.providers = {"cpu", "system"};
  config.plugin_paths = {"./build/sure_smartie_demo_plugin.so"};
  config.screens = {
      ScreenDefinition{
          .name = "summary",
          .interval = 2200ms,
          .lines = {
              "CPU {cpu.load}%",
              "BAR {bar:cpu.load,4}",
              "{system.time}",
              "{system.hostname}",
          },
      },
  };

  const auto roundtrip_path = std::filesystem::temp_directory_path() /
                              ("sure-smartie-roundtrip-" +
                               std::to_string(::getpid()) + ".json");
  ConfigSerializer::saveToFile(config, roundtrip_path);
  const auto loaded = ConfigLoader::loadFromFile(roundtrip_path);
  std::filesystem::remove(roundtrip_path);

  assert(loaded.device == config.device);
  assert(loaded.baudrate == config.baudrate);
  assert(loaded.refresh_interval == config.refresh_interval);
  assert(loaded.display.type == config.display.type);
  assert(loaded.display.cols == config.display.cols);
  assert(loaded.display.rows == config.display.rows);
  assert(loaded.display.backlight == config.display.backlight);
  assert(loaded.display.contrast == config.display.contrast);
  assert(loaded.display.brightness == config.display.brightness);
  assert(loaded.providers == config.providers);
  assert(loaded.plugin_paths == config.plugin_paths);
  assert(loaded.screens.size() == config.screens.size());
  assert(loaded.screens.front().name == config.screens.front().name);
  assert(loaded.screens.front().interval == config.screens.front().interval);
  assert(loaded.screens.front().lines == config.screens.front().lines);

  AppConfig invalid = config;
  invalid.display.type = "mystery";
  invalid.display.cols = 0;
  invalid.display.brightness = 999;
  invalid.providers = {"missing-provider"};
  invalid.screens = {
      ScreenDefinition{
          .name = "",
          .interval = 0ms,
          .lines = {
              "{bar:cpu.load,x}",
              "{",
          },
      },
  };

  const auto diagnostics = ConfigValidator::validate(invalid);
  assert(containsDiagnostic(diagnostics, DiagnosticSeverity::error, "display.type"));
  assert(containsDiagnostic(diagnostics, DiagnosticSeverity::error, "display.cols"));
  assert(containsDiagnostic(diagnostics, DiagnosticSeverity::error, "display.brightness"));
  assert(containsDiagnostic(diagnostics, DiagnosticSeverity::error, "providers[0]"));
  assert(containsDiagnostic(diagnostics, DiagnosticSeverity::error, "screens[0].interval_ms"));
  assert(containsDiagnostic(diagnostics, DiagnosticSeverity::error, "screens[0].lines[0]"));
  assert(containsDiagnostic(diagnostics, DiagnosticSeverity::error, "screens[0].lines[1]"));

  MetricMap metrics{
      {"cpu.load", "42"},
      {"system.time", "10:15"},
      {"system.hostname", "node"},
  };
  PreviewFrameRenderer renderer;
  sure_smartie::engine::TemplateEngine template_engine;
  const auto expected =
      template_engine.render(config.screens.front(), metrics, {20, 4});
  const auto actual = renderer.renderScreen(config, metrics, 0);
  assert(expected == actual);

  sure_smartie::providers::CpuProvider cpu_provider;
  sure_smartie::providers::GpuProvider gpu_provider;
  MetricMap collected_metrics;
  cpu_provider.collect(collected_metrics);
  gpu_provider.collect(collected_metrics);

  assert(collected_metrics.contains("cpu.power_w"));
  assert(collected_metrics.contains("cpu.fan_rpm"));
  assert(collected_metrics.contains("cpu.fan_percent"));
  assert(collected_metrics.contains("gpu.power_w"));
  assert(collected_metrics.contains("gpu.fan_rpm"));
  assert(collected_metrics.contains("gpu.fan_percent"));

  return 0;
}
