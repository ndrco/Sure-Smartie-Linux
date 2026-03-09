#include "sure_smartie/core/App.hpp"

#include <csignal>
#include <exception>
#include <iostream>
#include <iterator>
#include <thread>
#include <unordered_set>

#include "sure_smartie/display/DisplayFactory.hpp"
#include "sure_smartie/core/Logger.hpp"
#include "sure_smartie/plugins/PluginLoader.hpp"
#include "sure_smartie/providers/BuiltinProviderFactory.hpp"
#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::core {
namespace {

std::atomic_bool* g_stop_requested = nullptr;

std::chrono::milliseconds normalizedRefreshInterval(
    std::chrono::milliseconds interval) {
  return std::max(interval, std::chrono::milliseconds{100});
}

void onSignal([[maybe_unused]] int signal) {
  if (g_stop_requested != nullptr) {
    g_stop_requested->store(true);
  }
}

std::vector<std::unique_ptr<providers::IProvider>> createProviders(
    const AppConfig& config) {
  std::vector<std::unique_ptr<providers::IProvider>> providers;
  std::unordered_set<std::string> seen_builtin;

  for (const auto& provider_name : config.providers) {
    if (!seen_builtin.insert(provider_name).second) {
      continue;
    }

    try {
      providers.push_back(providers::createBuiltinProvider(provider_name));
    } catch (const std::exception& error) {
      logMessage(
          LogLevel::warn,
          "provider",
          "provider initialization failed",
          {
              {"provider", provider_name},
              {"error", error.what()},
          });
    }
  }

  for (const auto& plugin_path : config.plugin_paths) {
    try {
      auto plugin_providers = plugins::loadProviderPlugins({plugin_path});
      providers.insert(providers.end(),
                       std::make_move_iterator(plugin_providers.begin()),
                       std::make_move_iterator(plugin_providers.end()));
    } catch (const std::exception& error) {
      logMessage(
          LogLevel::warn,
          "plugin",
          "plugin loading failed",
          {
              {"path", plugin_path},
              {"error", error.what()},
          });
    }
  }

  return providers;
}

}  // namespace

App::App(AppConfig config, RuntimeOptions options)
    : config_(std::move(config)),
      options_(options),
      display_(display::createDisplay(config_, options_)),
      providers_(createProviders(config_)),
      screen_manager_(config_.screens) {}

MetricMap App::collectMetrics() {
  MetricMap metrics;

  for (const auto& provider : providers_) {
    try {
      provider->collect(metrics);
    } catch (const std::exception& error) {
      logMessage(
          LogLevel::warn,
          "provider",
          "provider collection failed",
          {
              {"provider", provider->name()},
              {"error", error.what()},
          });
      metrics["provider." + provider->name() + ".status"] = "error";
    }
  }

  return metrics;
}

void App::renderOnce() {
  const auto now = std::chrono::steady_clock::now();
  const auto metrics = collectMetrics();
  const auto& screen = screen_manager_.current(now);
  const auto frame =
      template_engine_.render(screen, metrics, display_->geometry());

  display_->initialize();
  display_->render(frame);
}

int App::run() {
  const auto previous_handler = std::signal(SIGINT, onSignal);
  g_stop_requested = &stop_requested_;
  const auto refresh_interval = normalizedRefreshInterval(config_.refresh_interval);

  try {
    if (options_.once) {
      renderOnce();
      logMessage(LogLevel::info, "app", "single render completed");
      g_stop_requested = nullptr;
      std::signal(SIGINT, previous_handler);
      return 0;
    }

    logMessage(
        LogLevel::info,
        "app",
        "entering render loop",
        {
            {"refresh_ms", std::to_string(refresh_interval.count())},
            {"providers", std::to_string(providers_.size())},
            {"screens", std::to_string(config_.screens.size())},
        });

    auto next_tick = std::chrono::steady_clock::now();
    while (!stop_requested_.load()) {
      try {
        renderOnce();
      } catch (const std::exception& error) {
        logMessage(
            LogLevel::error,
            "app",
            "render cycle failed",
            {{"error", error.what()}});
      }

      next_tick += refresh_interval;
      std::this_thread::sleep_until(next_tick);

      const auto now = std::chrono::steady_clock::now();
      if (now - next_tick > refresh_interval) {
        next_tick = now;
      }
    }
  } catch (...) {
    g_stop_requested = nullptr;
    std::signal(SIGINT, previous_handler);
    throw;
  }

  logMessage(LogLevel::info, "app", "stopping render loop");
  g_stop_requested = nullptr;
  std::signal(SIGINT, previous_handler);
  return 0;
}

}  // namespace sure_smartie::core
