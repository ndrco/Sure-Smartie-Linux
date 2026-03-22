#include "sure_smartie/core/App.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <csignal>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <thread>
#include <unordered_set>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

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
      providers.push_back(providers::createBuiltinProvider(provider_name, config));
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

std::array<std::uint8_t, 8> toProtocolGlyphPattern(const GlyphPattern& pattern) {
  std::array<std::uint8_t, 8> bytes{};
  for (std::size_t row = 0; row < pattern.size(); ++row) {
    bytes[row] = static_cast<std::uint8_t>(std::clamp(pattern[row], 0, 0x1F));
  }
  return bytes;
}

Frame blankFrameForGeometry(DisplayGeometry geometry) {
  return Frame(geometry.rows, std::string(geometry.cols, ' '));
}

std::string trimWhitespace(std::string value) {
  const auto first =
      std::find_if_not(value.begin(), value.end(), [](unsigned char symbol) {
        return std::isspace(symbol) != 0;
      });
  if (first == value.end()) {
    return {};
  }

  const auto last =
      std::find_if_not(value.rbegin(), value.rend(), [](unsigned char symbol) {
        return std::isspace(symbol) != 0;
      }).base();
  return std::string(first, last);
}

std::string readControlRequestLine(int client_fd) {
  std::array<char, 256> buffer{};
  std::string request;

  while (request.find('\n') == std::string::npos) {
    const ssize_t bytes = ::read(client_fd, buffer.data(), buffer.size());
    if (bytes == 0) {
      break;
    }
    if (bytes < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error(
          "control socket read failed: " + std::string(std::strerror(errno)));
    }

    request.append(buffer.data(), static_cast<std::size_t>(bytes));
    if (request.size() > 8192) {
      throw std::runtime_error("control socket request is unexpectedly large");
    }
  }

  if (const auto newline = request.find('\n'); newline != std::string::npos) {
    request.resize(newline);
  }
  return trimWhitespace(std::move(request));
}

void writeControlResponse(int client_fd, std::string_view response) {
  std::string payload(response);
  payload.push_back('\n');

  const char* cursor = payload.data();
  std::size_t remaining = payload.size();
  while (remaining > 0) {
    const ssize_t written = ::write(client_fd, cursor, remaining);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error(
          "control socket write failed: " + std::string(std::strerror(errno)));
    }
    cursor += written;
    remaining -= static_cast<std::size_t>(written);
  }
}

}  // namespace

App::App(AppConfig config, RuntimeOptions options)
    : config_(std::move(config)),
      options_(options),
      display_(display::createDisplay(config_, options_)),
      providers_(createProviders(config_)),
      screen_manager_(config_.screens, options_.auto_screen_rotation) {
  if (options_.initial_screen_selector.has_value() &&
      !options_.initial_screen_selector->empty()) {
    const auto now = std::chrono::steady_clock::now();
    if (!screen_manager_.setCurrentScreen(*options_.initial_screen_selector, now)) {
      throw std::invalid_argument(
          "Unknown screen selector: " + *options_.initial_screen_selector);
    }
  }

  setupScreenControlSocket();
}

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
  processScreenControlCommands(now);
  const auto metrics = collectMetrics();
  const auto& screen = screen_manager_.current(now);
  const auto rendered =
      template_engine_.renderDetailed(screen, metrics, display_->geometry(), config_.custom_glyphs);

  display_->initialize();
  for (std::size_t slot = 0; slot < rendered.glyphs.size(); ++slot) {
    if (!rendered.glyphs[slot].active) {
      continue;
    }

    display_->uploadCustomCharacter(static_cast<std::uint8_t>(slot),
                                    toProtocolGlyphPattern(rendered.glyphs[slot].pattern));
  }
  display_->render(rendered.frame);
}

void App::setupScreenControlSocket() {
  if (options_.screen_control_socket_path.empty()) {
    return;
  }

  const auto socket_parent = options_.screen_control_socket_path.parent_path();
  if (!socket_parent.empty()) {
    std::error_code error;
    std::filesystem::create_directories(socket_parent, error);
    if (error) {
      logMessage(
          LogLevel::warn,
          "screen",
          "unable to create screen control socket directory",
          {
              {"path", socket_parent.string()},
              {"error", error.message()},
          });
      return;
    }
  }

  sockaddr_un address{};
  if (options_.screen_control_socket_path.string().size() >= sizeof(address.sun_path)) {
    logMessage(
        LogLevel::warn,
        "screen",
        "screen control socket path is too long",
        {
            {"path", options_.screen_control_socket_path.string()},
        });
    return;
  }

  const int server_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (server_fd < 0) {
    logMessage(
        LogLevel::warn,
        "screen",
        "unable to create screen control socket",
        {
            {"error", std::strerror(errno)},
        });
    return;
  }

  const int current_flags = ::fcntl(server_fd, F_GETFD);
  if (current_flags >= 0) {
    ::fcntl(server_fd, F_SETFD, current_flags | FD_CLOEXEC);
  }

  ::unlink(options_.screen_control_socket_path.c_str());

  address.sun_family = AF_UNIX;
  std::strncpy(address.sun_path,
               options_.screen_control_socket_path.c_str(),
               sizeof(address.sun_path) - 1);
  address.sun_path[sizeof(address.sun_path) - 1] = '\0';

  if (::bind(server_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
    logMessage(
        LogLevel::warn,
        "screen",
        "unable to bind screen control socket",
        {
            {"path", options_.screen_control_socket_path.string()},
            {"error", std::strerror(errno)},
        });
    ::close(server_fd);
    return;
  }

  if (::chmod(options_.screen_control_socket_path.c_str(), 0666) != 0) {
    logMessage(
        LogLevel::warn,
        "screen",
        "unable to set permissions on screen control socket",
        {
            {"path", options_.screen_control_socket_path.string()},
            {"error", std::strerror(errno)},
        });
  }

  if (::listen(server_fd, 8) != 0) {
    logMessage(
        LogLevel::warn,
        "screen",
        "unable to listen on screen control socket",
        {
            {"path", options_.screen_control_socket_path.string()},
            {"error", std::strerror(errno)},
        });
    ::close(server_fd);
    std::error_code error;
    std::filesystem::remove(options_.screen_control_socket_path, error);
    return;
  }

  screen_control_socket_fd_ = server_fd;
  screen_control_socket_path_ = options_.screen_control_socket_path;
  logMessage(
      LogLevel::info,
      "screen",
      "screen control socket ready",
      {
          {"path", screen_control_socket_path_.string()},
      });
}

void App::processScreenControlCommands(std::chrono::steady_clock::time_point now) {
  if (screen_control_socket_fd_ < 0) {
    return;
  }

  while (true) {
    const int client_fd = ::accept(screen_control_socket_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }

      logMessage(
          LogLevel::warn,
          "screen",
          "failed to accept control socket client",
          {
              {"error", std::strerror(errno)},
          });
      break;
    }

    try {
      const std::string selector = readControlRequestLine(client_fd);
      if (selector.empty()) {
        writeControlResponse(client_fd, "error:empty-selector");
      } else if (screen_manager_.autoRotationEnabled()) {
        writeControlResponse(client_fd, "error:automatic-screen-rotation-enabled");
      } else if (screen_manager_.setCurrentScreen(selector, now)) {
        writeControlResponse(client_fd, "ok");
        logMessage(
            LogLevel::info,
            "screen",
            "screen changed from control socket command",
            {
                {"selector", selector},
                {"screen_index", std::to_string(screen_manager_.currentIndex() + 1)},
            });
      } else {
        writeControlResponse(client_fd, "error:unknown-screen-selector");
        logMessage(
            LogLevel::warn,
            "screen",
            "invalid screen selector from control socket",
            {
                {"selector", selector},
            });
      }
    } catch (const std::exception& error) {
      try {
        writeControlResponse(client_fd, "error:internal-server-error");
      } catch (...) {
      }
      logMessage(
          LogLevel::warn,
          "screen",
          "control socket command processing failed",
          {
              {"error", error.what()},
          });
    }

    ::close(client_fd);
  }
}

void App::teardownScreenControlSocket() {
  if (screen_control_socket_fd_ >= 0) {
    ::close(screen_control_socket_fd_);
    screen_control_socket_fd_ = -1;
  }

  if (!screen_control_socket_path_.empty()) {
    std::error_code error;
    std::filesystem::remove(screen_control_socket_path_, error);
    screen_control_socket_path_.clear();
  }
}

void App::shutdownDisplay() {
  if (options_.force_stdout_display || config_.display.type == "stdout") {
    display_->release();
    return;
  }

  display_->initialize();
  display_->render(blankFrameForGeometry(display_->geometry()));
  display_->setBacklight(false);
  display_->release();
}

int App::run() {
  const auto previous_handler = std::signal(SIGINT, onSignal);
  const auto previous_term_handler = std::signal(SIGTERM, onSignal);
  g_stop_requested = &stop_requested_;
  const auto refresh_interval = normalizedRefreshInterval(config_.refresh_interval);

  try {
    if (options_.once) {
      try {
        renderOnce();
      } catch (...) {
        display_->release();
        teardownScreenControlSocket();
        throw;
      }
      display_->release();
      teardownScreenControlSocket();
      logMessage(LogLevel::info, "app", "single render completed");
      g_stop_requested = nullptr;
      std::signal(SIGINT, previous_handler);
      std::signal(SIGTERM, previous_term_handler);
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
        display_->release();
      }

      next_tick += refresh_interval;
      std::this_thread::sleep_until(next_tick);

      const auto now = std::chrono::steady_clock::now();
      if (now - next_tick > refresh_interval) {
        next_tick = now;
      }
    }
  } catch (...) {
    teardownScreenControlSocket();
    g_stop_requested = nullptr;
    std::signal(SIGINT, previous_handler);
    std::signal(SIGTERM, previous_term_handler);
    throw;
  }

  try {
    shutdownDisplay();
  } catch (const std::exception& error) {
    logMessage(
        LogLevel::warn,
        "display",
        "display shutdown cleanup failed",
        {{"error", error.what()}});
    display_->release();
  }

  teardownScreenControlSocket();
  logMessage(LogLevel::info, "app", "stopping render loop");
  g_stop_requested = nullptr;
  std::signal(SIGINT, previous_handler);
  std::signal(SIGTERM, previous_term_handler);
  return 0;
}

}  // namespace sure_smartie::core
