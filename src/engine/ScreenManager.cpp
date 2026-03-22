#include "sure_smartie/engine/ScreenManager.hpp"

#include <algorithm>
#include <charconv>
#include <optional>
#include <stdexcept>

namespace sure_smartie::engine {
namespace {

std::chrono::milliseconds normalizedInterval(std::chrono::milliseconds interval) {
  return std::max(interval, std::chrono::milliseconds{100});
}

void updateNextSwitch(const core::ScreenDefinition& screen,
                      std::chrono::steady_clock::time_point now,
                      std::chrono::steady_clock::time_point& next_switch) {
  next_switch = now + normalizedInterval(screen.interval);
}

std::optional<std::size_t> parseOneBasedIndex(std::string_view value) {
  if (value.empty()) {
    return std::nullopt;
  }

  std::size_t parsed = 0;
  const auto result =
      std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
    return std::nullopt;
  }
  if (parsed == 0) {
    return std::nullopt;
  }

  return parsed - 1;
}

std::optional<std::size_t> parseExplicitZeroBasedIndex(std::string_view value) {
  static constexpr std::string_view kPrefix = "index:";
  if (!value.starts_with(kPrefix)) {
    return std::nullopt;
  }

  const std::string_view raw_index = value.substr(kPrefix.size());
  if (raw_index.empty()) {
    return std::nullopt;
  }

  std::size_t parsed = 0;
  const auto result =
      std::from_chars(raw_index.data(), raw_index.data() + raw_index.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != raw_index.data() + raw_index.size()) {
    return std::nullopt;
  }

  return parsed;
}

}  // namespace

ScreenManager::ScreenManager(std::vector<core::ScreenDefinition> screens,
                             bool auto_rotation_enabled)
    : screens_(std::move(screens)),
      auto_rotation_enabled_(auto_rotation_enabled) {
  if (screens_.empty()) {
    throw std::invalid_argument("At least one screen must be configured");
  }
}

const core::ScreenDefinition& ScreenManager::current(
    std::chrono::steady_clock::time_point now) {
  if (!started_) {
    started_ = true;
    updateNextSwitch(screens_[current_index_], now, next_switch_);
    return screens_[current_index_];
  }

  if (!auto_rotation_enabled_) {
    return screens_[current_index_];
  }

  if (screens_.size() == 1) {
    if (now >= next_switch_) {
      updateNextSwitch(screens_[current_index_], now, next_switch_);
    }
    return screens_[current_index_];
  }

  while (now >= next_switch_) {
    current_index_ = (current_index_ + 1) % screens_.size();
    next_switch_ += normalizedInterval(screens_[current_index_].interval);
  }

  return screens_[current_index_];
}

bool ScreenManager::setCurrentScreen(std::string_view selector,
                                     std::chrono::steady_clock::time_point now) {
  if (selector == "next") {
    const std::size_t next_index = (current_index_ + 1) % screens_.size();
    return setCurrentScreenIndex(next_index, now);
  }

  if (const auto explicit_zero_based = parseExplicitZeroBasedIndex(selector);
      explicit_zero_based.has_value()) {
    return setCurrentScreenIndex(*explicit_zero_based, now);
  }

  if (const auto one_based = parseOneBasedIndex(selector); one_based.has_value()) {
    return setCurrentScreenIndex(*one_based, now);
  }

  for (std::size_t index = 0; index < screens_.size(); ++index) {
    if (screens_[index].name == selector) {
      return setCurrentScreenIndex(index, now);
    }
  }

  return false;
}

bool ScreenManager::setCurrentScreenIndex(
    std::size_t index,
    std::chrono::steady_clock::time_point now) {
  if (index >= screens_.size()) {
    return false;
  }

  current_index_ = index;
  started_ = true;
  updateNextSwitch(screens_[current_index_], now, next_switch_);
  return true;
}

bool ScreenManager::autoRotationEnabled() const noexcept {
  return auto_rotation_enabled_;
}

std::size_t ScreenManager::currentIndex() const noexcept {
  return current_index_;
}

}  // namespace sure_smartie::engine
