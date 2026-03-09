#include "sure_smartie/core/App.hpp"

#include <csignal>
#include <exception>
#include <iostream>
#include <thread>

#include "sure_smartie/display/DisplayFactory.hpp"
#include "sure_smartie/providers/BuiltinProviderFactory.hpp"
#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::core {
namespace {

std::atomic_bool* g_stop_requested = nullptr;

void onSignal([[maybe_unused]] int signal) {
  if (g_stop_requested != nullptr) {
    g_stop_requested->store(true);
  }
}

}  // namespace

App::App(AppConfig config, RuntimeOptions options)
    : config_(std::move(config)),
      options_(options),
      display_(display::createDisplay(config_, options_)),
      providers_(providers::createBuiltinProviders(config_.providers)),
      screen_manager_(config_.screens) {}

MetricMap App::collectMetrics() {
  MetricMap metrics;

  for (const auto& provider : providers_) {
    try {
      provider->collect(metrics);
    } catch (const std::exception& error) {
      std::cerr << "provider '" << provider->name()
                << "' failed: " << error.what() << '\n';
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

  try {
    if (options_.once) {
      renderOnce();
      g_stop_requested = nullptr;
      std::signal(SIGINT, previous_handler);
      return 0;
    }

    auto next_tick = std::chrono::steady_clock::now();
    while (!stop_requested_.load()) {
      try {
        renderOnce();
      } catch (const std::exception& error) {
        std::cerr << "render cycle failed: " << error.what() << '\n';
      }

      next_tick += config_.refresh_interval;
      std::this_thread::sleep_until(next_tick);

      const auto now = std::chrono::steady_clock::now();
      if (now - next_tick > config_.refresh_interval) {
        next_tick = now;
      }
    }
  } catch (...) {
    g_stop_requested = nullptr;
    std::signal(SIGINT, previous_handler);
    throw;
  }

  g_stop_requested = nullptr;
  std::signal(SIGINT, previous_handler);
  return 0;
}

}  // namespace sure_smartie::core
