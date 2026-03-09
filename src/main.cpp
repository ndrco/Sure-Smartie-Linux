#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "sure_smartie/core/App.hpp"
#include "sure_smartie/core/Config.hpp"

namespace {

void printUsage(const char* program_name) {
  std::cout << "Usage: " << program_name
            << " [--config <path>] [--once] [--stdout]\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path config_path{"configs/sure-example.json"};
  sure_smartie::core::RuntimeOptions options;

  try {
    for (int index = 1; index < argc; ++index) {
      const std::string_view arg{argv[index]};

      if (arg == "--config") {
        if (index + 1 >= argc) {
          throw std::invalid_argument("--config requires a path");
        }
        config_path = argv[++index];
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

      if (arg == "--help" || arg == "-h") {
        printUsage(argv[0]);
        return 0;
      }

      throw std::invalid_argument("Unknown argument: " + std::string(arg));
    }

    auto config = sure_smartie::core::ConfigLoader::loadFromFile(config_path);
    sure_smartie::core::App app(std::move(config), options);
    return app.run();
  } catch (const std::exception& error) {
    std::cerr << "fatal: " << error.what() << '\n';
    printUsage(argv[0]);
    return 1;
  }
}
