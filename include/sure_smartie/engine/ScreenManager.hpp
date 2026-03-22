#pragma once

#include <chrono>
#include <string_view>
#include <vector>

#include "sure_smartie/core/Types.hpp"

namespace sure_smartie::engine {

class ScreenManager {
 public:
  explicit ScreenManager(std::vector<core::ScreenDefinition> screens,
                         bool auto_rotation_enabled = true);

  const core::ScreenDefinition& current(std::chrono::steady_clock::time_point now);
  bool setCurrentScreen(std::string_view selector,
                        std::chrono::steady_clock::time_point now);
  bool setCurrentScreenIndex(std::size_t index,
                             std::chrono::steady_clock::time_point now);
  bool autoRotationEnabled() const noexcept;
  std::size_t currentIndex() const noexcept;

 private:
  std::vector<core::ScreenDefinition> screens_;
  bool auto_rotation_enabled_{true};
  std::size_t current_index_{0};
  std::chrono::steady_clock::time_point next_switch_{};
  bool started_{false};
};

}  // namespace sure_smartie::engine
