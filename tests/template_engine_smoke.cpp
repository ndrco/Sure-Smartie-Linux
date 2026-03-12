#include <cassert>
#include <chrono>
#include <string>
#include <vector>

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
  assert(static_cast<unsigned char>(bar_frame[0][4]) == 5);
  assert(static_cast<unsigned char>(bar_frame[0][5]) == 3);

  sure_smartie::core::ScreenDefinition aligned_screen{
      .name = "aligned",
      .interval = std::chrono::milliseconds{5},
      .lines = {"CPU {cpu.load}%{at:12}{ram.percent}%"},
  };
  const auto aligned_frame =
      engine.render(aligned_screen, metrics, {.cols = 20, .rows = 1});
  assert(aligned_frame.size() == 1);
  assert(aligned_frame[0] == "CPU 42%    73%      ");

  sure_smartie::engine::ScreenManager manager({first, second});
  const auto start = std::chrono::steady_clock::now();
  assert(manager.current(start).name == "first");
  assert(manager.current(start + std::chrono::milliseconds{120}).name == "second");

  return 0;
}
