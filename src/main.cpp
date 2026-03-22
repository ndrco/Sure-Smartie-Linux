#include <filesystem>
#include <cstdlib>
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "sure_smartie/core/App.hpp"
#include "sure_smartie/core/Config.hpp"
#include "sure_smartie/core/ConfigValidator.hpp"
#include "sure_smartie/display/DisplayFactory.hpp"
#include "sure_smartie/core/Logger.hpp"

namespace {

std::filesystem::path defaultConfigPath() {
  if (const char* config_env = std::getenv("SURE_SMARTIE_CONFIG");
      config_env != nullptr && *config_env != '\0') {
    return config_env;
  }

  const std::filesystem::path local_example{"configs/sure-example.json"};
  if (std::filesystem::exists(local_example)) {
    return local_example;
  }

#ifdef SURE_SMARTIE_DEFAULT_CONFIG_PATH
  const std::filesystem::path installed_config{SURE_SMARTIE_DEFAULT_CONFIG_PATH};
  if (std::filesystem::exists(installed_config)) {
    return installed_config;
  }

  const std::filesystem::path installed_example =
      installed_config.string() + ".example";
  if (std::filesystem::exists(installed_example)) {
    return installed_example;
  }

  return installed_config;
#else
  return "/etc/sure-smartie-linux/config.json";
#endif
}

std::filesystem::path defaultScreenControlSocketPath() {
  if (const char* socket_env = std::getenv("SURE_SMARTIE_SCREEN_CONTROL_SOCKET");
      socket_env != nullptr && *socket_env != '\0') {
    return socket_env;
  }

  if (const char* command_file_env = std::getenv("SURE_SMARTIE_SCREEN_COMMAND_FILE");
      command_file_env != nullptr && *command_file_env != '\0') {
    return command_file_env;
  }

  return "/run/sure-smartie-linux/control.sock";
}

void configureLoggingFromEnvironment() {
  if (const char* level_env = std::getenv("SURE_SMARTIE_LOG_LEVEL");
      level_env != nullptr && *level_env != '\0') {
    sure_smartie::core::setLogLevel(
        sure_smartie::core::parseLogLevel(level_env));
  }
}

void printConfigSummary(const sure_smartie::core::AppConfig& config) {
  std::cout << "config ok\n"
            << "device: " << config.device << '\n'
            << "display: " << config.display.type << ' ' << config.display.cols << 'x'
            << config.display.rows << '\n'
            << "providers: " << config.providers.size() << '\n'
            << "plugins: " << config.plugin_paths.size() << '\n'
            << "screens: " << config.screens.size() << '\n';
}

std::string severityToString(
    sure_smartie::core::DiagnosticSeverity severity) {
  switch (severity) {
    case sure_smartie::core::DiagnosticSeverity::info:
      return "info";
    case sure_smartie::core::DiagnosticSeverity::warning:
      return "warning";
    case sure_smartie::core::DiagnosticSeverity::error:
      return "error";
  }

  return "unknown";
}

bool hasErrors(
    const std::vector<sure_smartie::core::Diagnostic>& diagnostics) {
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.severity == sure_smartie::core::DiagnosticSeverity::error) {
      return true;
    }
  }

  return false;
}

void printDiagnostics(
    std::ostream& stream,
    const std::vector<sure_smartie::core::Diagnostic>& diagnostics) {
  if (diagnostics.empty()) {
    stream << "validation: ok\n";
    return;
  }

  for (const auto& diagnostic : diagnostics) {
    stream << severityToString(diagnostic.severity) << ": "
           << diagnostic.field_path << ": " << diagnostic.message << '\n';
  }
}

int runBacklightCommand(sure_smartie::core::AppConfig config,
                        sure_smartie::core::RuntimeOptions options,
                        bool backlight_on) {
  config.display.backlight = backlight_on;

  auto display = sure_smartie::display::createDisplay(config, options);
  display->initialize();
  display->release();

  sure_smartie::core::logMessage(
      sure_smartie::core::LogLevel::info,
      "main",
      "backlight command applied",
      {{"backlight", backlight_on ? "on" : "off"}});
  return 0;
}

std::string readSingleLineFromSocket(int fd) {
  std::array<char, 512> buffer{};
  std::string response;
  while (response.find('\n') == std::string::npos) {
    const ssize_t bytes_read = ::read(fd, buffer.data(), buffer.size());
    if (bytes_read == 0) {
      break;
    }
    if (bytes_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error("Unable to read control socket response: " +
                               std::string(std::strerror(errno)));
    }
    response.append(buffer.data(), static_cast<std::size_t>(bytes_read));
    if (response.size() > 8192) {
      throw std::runtime_error("Control socket response is unexpectedly large");
    }
  }

  if (const auto newline_pos = response.find('\n'); newline_pos != std::string::npos) {
    response.resize(newline_pos);
  }
  return response;
}

void writeAllToSocket(int fd, std::string_view payload) {
  const char* cursor = payload.data();
  std::size_t remaining = payload.size();
  while (remaining > 0) {
    const ssize_t written = ::write(fd, cursor, remaining);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error("Unable to send control socket request: " +
                               std::string(std::strerror(errno)));
    }
    cursor += written;
    remaining -= static_cast<std::size_t>(written);
  }
}

int runSetScreenCommand(const std::filesystem::path& socket_path,
                        std::string selector) {
  if (selector.empty()) {
    throw std::invalid_argument("--set-screen requires a non-empty selector");
  }

  if (socket_path.empty()) {
    throw std::invalid_argument(
        "--set-screen requires a non-empty control socket path");
  }

  sockaddr_un address{};
  if (socket_path.string().size() >= sizeof(address.sun_path)) {
    throw std::runtime_error(
        "Control socket path is too long: " + socket_path.string());
  }

  const int socket_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    throw std::runtime_error(
        "Unable to create control socket client: " +
        std::string(std::strerror(errno)));
  }
  struct SocketFdGuard {
    int fd{-1};
    ~SocketFdGuard() {
      if (fd >= 0) {
        ::close(fd);
      }
    }
  };
  SocketFdGuard guard{socket_fd};

  address.sun_family = AF_UNIX;
  std::strncpy(address.sun_path,
               socket_path.c_str(),
               sizeof(address.sun_path) - 1);
  address.sun_path[sizeof(address.sun_path) - 1] = '\0';

  if (::connect(socket_fd,
                reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)) != 0) {
    const std::string message = std::strerror(errno);
    throw std::runtime_error(
        "Unable to connect to screen control socket " + socket_path.string() +
        ": " + message +
        ". Ensure sure-smartie-linux service is running.");
  }

  const std::string request = selector + "\n";
  writeAllToSocket(socket_fd, request);
  const std::string response = readSingleLineFromSocket(socket_fd);
  guard.fd = -1;
  ::close(socket_fd);

  if (response == "ok") {
    sure_smartie::core::logMessage(
        sure_smartie::core::LogLevel::info,
        "main",
        "screen command sent",
        {
            {"selector", std::move(selector)},
            {"socket", socket_path.string()},
        });
    return 0;
  }

  if (response.rfind("error:", 0) == 0) {
    throw std::runtime_error("screen command rejected: " + response.substr(6));
  }

  throw std::runtime_error("unexpected control socket response: " + response);
}

void printUsage(const char* program_name) {
  std::cout << "Usage: " << program_name
            << " [--config <path>] [--device <path>] [--once] [--stdout]"
               " [--validate-config] [--backlight <on|off>]"
               " [--no-screen-rotation] [--screen <name|N|index:N|next>]"
               " [--screen-control-socket <path>] [--set-screen <name|N|index:N|next>]"
               " [--log-level <trace|debug|info|warn|error>]\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path config_path{defaultConfigPath()};
  std::string device_override;
  std::optional<bool> backlight_override;
  std::optional<std::string> set_screen_selector;
  bool disable_screen_rotation_override = false;
  sure_smartie::core::RuntimeOptions options;
  options.screen_control_socket_path = defaultScreenControlSocketPath();

  try {
    configureLoggingFromEnvironment();

    for (int index = 1; index < argc; ++index) {
      const std::string_view arg{argv[index]};

      if (arg == "--config") {
        if (index + 1 >= argc) {
          throw std::invalid_argument("--config requires a path");
        }
        config_path = argv[++index];
        continue;
      }

      if (arg == "--device") {
        if (index + 1 >= argc) {
          throw std::invalid_argument("--device requires a path");
        }
        device_override = argv[++index];
        continue;
      }

      if (arg == "--once") {
        options.once = true;
        continue;
      }

      if (arg == "--stdout") {
        options.force_stdout_display = true;
        continue;
      }

      if (arg == "--validate-config") {
        options.validate_config_only = true;
        continue;
      }

      if (arg == "--no-screen-rotation") {
        options.auto_screen_rotation = false;
        disable_screen_rotation_override = true;
        continue;
      }

      if (arg == "--screen") {
        if (index + 1 >= argc) {
          throw std::invalid_argument("--screen requires a selector");
        }
        options.initial_screen_selector = argv[++index];
        continue;
      }

      if (arg == "--screen-control-socket" || arg == "--screen-command-file") {
        if (index + 1 >= argc) {
          throw std::invalid_argument(
              std::string(arg) + " requires a path");
        }
        options.screen_control_socket_path = argv[++index];
        continue;
      }

      if (arg == "--set-screen") {
        if (index + 1 >= argc) {
          throw std::invalid_argument("--set-screen requires a selector");
        }
        set_screen_selector = argv[++index];
        continue;
      }

      if (arg == "--backlight") {
        if (index + 1 >= argc) {
          throw std::invalid_argument("--backlight requires on or off");
        }

        const std::string_view value{argv[++index]};
        if (value == "on") {
          backlight_override = true;
        } else if (value == "off") {
          backlight_override = false;
        } else {
          throw std::invalid_argument("--backlight requires on or off");
        }
        continue;
      }

      if (arg == "--log-level") {
        if (index + 1 >= argc) {
          throw std::invalid_argument("--log-level requires a value");
        }
        sure_smartie::core::setLogLevel(
            sure_smartie::core::parseLogLevel(argv[++index]));
        continue;
      }

      if (arg == "--help" || arg == "-h") {
        printUsage(argv[0]);
        return 0;
      }

      throw std::invalid_argument("Unknown argument: " + std::string(arg));
    }

    if (set_screen_selector.has_value()) {
      return runSetScreenCommand(options.screen_control_socket_path,
                                 *set_screen_selector);
    }

    auto config = sure_smartie::core::ConfigLoader::loadFromFile(config_path);
    if (!device_override.empty()) {
      config.device = device_override;
    }
    if (!disable_screen_rotation_override) {
      options.auto_screen_rotation = config.auto_screen_rotation;
    }
    if (options.initial_screen_selector.has_value() && options.auto_screen_rotation) {
      throw std::invalid_argument(
          "--screen is available only when automatic screen rotation is disabled");
    }
    const auto diagnostics =
        sure_smartie::core::ConfigValidator::validate(config);
    if (options.validate_config_only) {
      printConfigSummary(config);
      printDiagnostics(std::cout, diagnostics);
      return hasErrors(diagnostics) ? 1 : 0;
    }

    if (!diagnostics.empty()) {
      printDiagnostics(std::cerr, diagnostics);
    }
    if (hasErrors(diagnostics)) {
      throw std::invalid_argument("configuration validation failed");
    }

    if (backlight_override.has_value()) {
      return runBacklightCommand(config, options, *backlight_override);
    }

    sure_smartie::core::logMessage(
        sure_smartie::core::LogLevel::info,
        "main",
        "starting application",
        {
            {"config", config_path.string()},
            {"device", config.device},
            {"display", config.display.type},
            {"once", options.once ? "true" : "false"},
        });

    sure_smartie::core::App app(std::move(config), options);
    return app.run();
  } catch (const std::exception& error) {
    sure_smartie::core::logMessage(
        sure_smartie::core::LogLevel::error,
        "main",
        "fatal error",
        {{"error", error.what()}});
    printUsage(argv[0]);
    return 1;
  }
}
