#pragma once

#include <vector>

#include "sure_smartie/core/Types.hpp"

namespace sure_smartie::core {

class ConfigValidator {
 public:
  static std::vector<Diagnostic> validate(const AppConfig& config);
};

}  // namespace sure_smartie::core
