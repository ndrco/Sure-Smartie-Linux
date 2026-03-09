#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "sure_smartie/core/App.hpp"
#include "sure_smartie/core/Config.hpp"
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
  return SURE_SMARTIE_DEFAULT_CONFIG_PATH;
#else
  return "/etc/sure-smartie-linux/config.json";
#endif
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

void printUsage(const char* program_name) {
  std::cout << "Usage: " << program_name
            << " [--config <path>] [--device <path>] [--once] [--stdout]"
               " [--validate-config] [--log-level <trace|debug|info|warn|error>]\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path config_path{defaultConfigPath()};
  std::string device_override;
  sure_smartie::core::RuntimeOptions options;

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

    auto config = sure_smartie::core::ConfigLoader::loadFromFile(config_path);
    if (!device_override.empty()) {
      config.device = device_override;
    }
    if (options.validate_config_only) {
      printConfigSummary(config);
      return 0;
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
