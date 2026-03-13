#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "sure_smartie/core/Types.hpp"
#include "sure_smartie/display/IDisplay.hpp"
#include "sure_smartie/engine/ScreenManager.hpp"
#include "sure_smartie/engine/TemplateEngine.hpp"
#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::core {

class App {
 public:
  App(AppConfig config, RuntimeOptions options);

  int run();

 private:
  MetricMap collectMetrics();
  void renderOnce();
  void shutdownDisplay();

  AppConfig config_;
  RuntimeOptions options_;
  std::unique_ptr<display::IDisplay> display_;
  std::vector<std::unique_ptr<providers::IProvider>> providers_;
  engine::TemplateEngine template_engine_;
  engine::ScreenManager screen_manager_;
  std::atomic_bool stop_requested_{false};
};

}  // namespace sure_smartie::core
