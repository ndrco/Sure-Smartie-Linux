#pragma once

#include <chrono>
#include <vector>

#include "sure_smartie/core/Types.hpp"

namespace sure_smartie::engine {

class ScreenManager {
 public:
  explicit ScreenManager(std::vector<core::ScreenDefinition> screens);

  const core::ScreenDefinition& current(std::chrono::steady_clock::time_point now);

 private:
  std::vector<core::ScreenDefinition> screens_;
  std::size_t current_index_{0};
  std::chrono::steady_clock::time_point next_switch_{};
  bool started_{false};
};

}  // namespace sure_smartie::engine
