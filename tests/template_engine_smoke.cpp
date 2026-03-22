#include <cassert>
#include <chrono>
#include <string>
#include <vector>

#include "sure_smartie/core/ConfigValidator.hpp"
#include "sure_smartie/core/Types.hpp"
#include "sure_smartie/engine/ScreenManager.hpp"
#include "sure_smartie/engine/TemplateEngine.hpp"

int main() {
  sure_smartie::engine::TemplateEngine engine;
  sure_smartie::core::ScreenDefinition first{
      .name = "first",
      .interval = std::chrono::milliseconds{5},
      .lines = {"CPU {cpu.load}%", "HOST {system.hostname}"},
  };
  sure_smartie::core::ScreenDefinition second{
      .name = "second",
      .interval = std::chrono::milliseconds{5},
      .lines = {"RAM {ram.percent}%"},
  };

  sure_smartie::core::MetricMap metrics{
      {"cpu.load", "42"},
      {"system.hostname", "box"},
      {"ram.percent", "73"},
  };

  const auto frame =
      engine.render(first, metrics, {.cols = 20, .rows = 4});
  assert(frame.size() == 4);
  assert(frame[0] == "CPU 42%             ");
  assert(frame[1] == "HOST box            ");
  assert(frame[2] == "                    ");

  sure_smartie::core::ScreenDefinition bar_screen{
      .name = "bars",
      .interval = std::chrono::milliseconds{5},
      .lines = {"CPU {bar:cpu.load,4} {cpu.load}%"},
  };
  const auto bar_frame =
      engine.render(bar_screen, metrics, {.cols = 20, .rows = 1});
  assert(bar_frame.size() == 1);
  assert(bar_frame[0].size() == 20);
  assert(bar_frame[0][4] == sure_smartie::core::kGlyphBar5);
  assert(bar_frame[0][5] == sure_smartie::core::kGlyphBar3);
  assert(bar_frame[0][6] == sure_smartie::core::kGlyphBarBase);
  assert(bar_frame[0][7] == sure_smartie::core::kGlyphBarBase);

  sure_smartie::core::ScreenDefinition aligned_screen{
      .name = "aligned",
      .interval = std::chrono::milliseconds{5},
      .lines = {"CPU {cpu.load}%{at:12}{ram.percent}%"},
  };
  const auto aligned_frame =
      engine.render(aligned_screen, metrics, {.cols = 20, .rows = 1});
  assert(aligned_frame.size() == 1);
  assert(aligned_frame[0] == "CPU 42%    73%      ");

  sure_smartie::core::ScreenDefinition forced_column_screen{
      .name = "forced-column",
      .interval = std::chrono::milliseconds{5},
      .lines = {"ABCDEFGHIJ{at:4}X"},
  };
  const auto forced_column_frame =
      engine.render(forced_column_screen, metrics, {.cols = 12, .rows = 1});
  assert(forced_column_frame.size() == 1);
  assert(forced_column_frame[0] == "ABCX        ");

  sure_smartie::core::MetricMap short_fallback_metrics{
      {"disk.1.device", "/dev/nvme3n1p2"},
      {"disk.1.mount", "/mnt/timeshift"},
  };
  sure_smartie::core::ScreenDefinition short_fallback_screen{
      .name = "short-fallback",
      .interval = std::chrono::milliseconds{5},
      .lines = {"{disk.1.device_short} {disk.1.mount_short}"},
  };
  const auto short_fallback_frame =
      engine.render(short_fallback_screen, short_fallback_metrics, {.cols = 20, .rows = 1});
  assert(short_fallback_frame.size() == 1);
  assert(short_fallback_frame[0] == "nvme3n1p2 timeshift ");

  const std::vector<sure_smartie::core::CustomGlyphDefinition> custom_glyphs{
      sure_smartie::core::CustomGlyphDefinition{
          .name = "heart",
          .pattern = {0x00, 0x0A, 0x1F, 0x1F, 0x1F, 0x0E, 0x04, 0x00},
      },
  };
  sure_smartie::core::ScreenDefinition custom_glyph_screen{
      .name = "custom",
      .interval = std::chrono::milliseconds{5},
      .lines = {"{glyph:heart}{bar:cpu.load,2}"},
  };
  const auto custom_rendered =
      engine.renderDetailed(custom_glyph_screen, metrics, {.cols = 8, .rows = 1}, custom_glyphs);
  assert(custom_rendered.frame.size() == 1);
  assert(static_cast<unsigned char>(custom_rendered.frame[0][0]) == 0);
  assert(static_cast<unsigned char>(custom_rendered.frame[0][1]) == 4);
  assert(static_cast<unsigned char>(custom_rendered.frame[0][2]) == 6);
  assert(custom_rendered.glyphs[0].active);
  assert(custom_rendered.glyphs[0].name == "heart");
  assert(custom_rendered.glyphs[0].pattern[2] == 0x1F);

  sure_smartie::core::AppConfig invalid_config;
  invalid_config.custom_glyphs = {
      sure_smartie::core::CustomGlyphDefinition{.name = "a"},
      sure_smartie::core::CustomGlyphDefinition{.name = "b"},
      sure_smartie::core::CustomGlyphDefinition{.name = "c"},
  };
  invalid_config.screens = {
      sure_smartie::core::ScreenDefinition{
          .name = "invalid",
          .interval = std::chrono::milliseconds{5},
          .lines = {"{bar:cpu.load,1}{glyph:a}{glyph:b}{glyph:c}"},
      },
  };
  const auto invalid_diagnostics =
      sure_smartie::core::ConfigValidator::validate(invalid_config);
  bool found_slot_error = false;
  for (const auto& diagnostic : invalid_diagnostics) {
    if (diagnostic.field_path == "screens[0].lines" &&
        diagnostic.severity == sure_smartie::core::DiagnosticSeverity::error) {
      found_slot_error = true;
      break;
    }
  }
  assert(found_slot_error);

  sure_smartie::engine::ScreenManager manager({first, second});
  const auto start = std::chrono::steady_clock::now();
  assert(manager.current(start).name == "first");
  assert(manager.current(start + std::chrono::milliseconds{120}).name == "second");

  sure_smartie::engine::ScreenManager manual_manager({first, second}, false);
  const auto manual_start = std::chrono::steady_clock::now();
  assert(manual_manager.current(manual_start).name == "first");
  assert(manual_manager.current(manual_start + std::chrono::milliseconds{120}).name ==
         "first");
  assert(manual_manager.setCurrentScreen("2", manual_start));
  assert(manual_manager.current(manual_start + std::chrono::milliseconds{200}).name ==
         "second");
  assert(manual_manager.setCurrentScreen("first", manual_start));
  assert(manual_manager.current(manual_start + std::chrono::milliseconds{220}).name ==
         "first");
  assert(manual_manager.setCurrentScreen("index:1", manual_start));
  assert(manual_manager.currentIndex() == 1);
  assert(manual_manager.setCurrentScreen("next", manual_start));
  assert(manual_manager.currentIndex() == 0);
  assert(!manual_manager.setCurrentScreen("99", manual_start));
  assert(!manual_manager.setCurrentScreen("missing", manual_start));

  return 0;
}
