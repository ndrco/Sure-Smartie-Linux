#include "sure_smartie/engine/ScreenManager.hpp"

#include <algorithm>
#include <stdexcept>

namespace sure_smartie::engine {
namespace {

std::chrono::milliseconds normalizedInterval(std::chrono::milliseconds interval) {
  return std::max(interval, std::chrono::milliseconds{100});
}

}  // namespace

ScreenManager::ScreenManager(std::vector<core::ScreenDefinition> screens)
    : screens_(std::move(screens)) {
  if (screens_.empty()) {
    throw std::invalid_argument("At least one screen must be configured");
  }
}

const core::ScreenDefinition& ScreenManager::current(
    std::chrono::steady_clock::time_point now) {
  if (!started_) {
    started_ = true;
    next_switch_ = now + normalizedInterval(screens_[current_index_].interval);
    return screens_[current_index_];
  }

  if (screens_.size() == 1) {
    if (now >= next_switch_) {
      next_switch_ = now + normalizedInterval(screens_[current_index_].interval);
    }
    return screens_[current_index_];
  }

  while (now >= next_switch_) {
    current_index_ = (current_index_ + 1) % screens_.size();
    next_switch_ += normalizedInterval(screens_[current_index_].interval);
  }

  return screens_[current_index_];
}

}  // namespace sure_smartie::engine
