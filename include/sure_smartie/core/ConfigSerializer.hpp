#pragma once

#include <filesystem>
#include <string>

#include "sure_smartie/core/Types.hpp"

namespace sure_smartie::core {

class ConfigSerializer {
 public:
  static std::string serialize(const AppConfig& config);
  static void saveToFile(const AppConfig& config, const std::filesystem::path& path);
};

}  // namespace sure_smartie::core
