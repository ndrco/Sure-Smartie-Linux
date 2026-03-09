#pragma once

#include <filesystem>

#include "sure_smartie/core/Types.hpp"

namespace sure_smartie::core {

class ConfigLoader {
 public:
  static AppConfig loadFromFile(const std::filesystem::path& path);
};

}  // namespace sure_smartie::core
