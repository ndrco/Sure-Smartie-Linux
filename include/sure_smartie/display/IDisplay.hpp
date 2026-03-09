#pragma once

#include <array>
#include <cstdint>

#include "sure_smartie/core/Types.hpp"

namespace sure_smartie::display {

class IDisplay {
 public:
  virtual ~IDisplay() = default;

  virtual core::DisplayGeometry geometry() const = 0;
  virtual void initialize() = 0;
  virtual void render(const core::Frame& frame) = 0;
  virtual void setBacklight(bool on) = 0;
  virtual void setContrast(std::uint8_t value) = 0;
  virtual void setBrightness(std::uint8_t value) = 0;
  virtual void uploadCustomCharacter(
      std::uint8_t index,
      const std::array<std::uint8_t, 8>& pattern) = 0;
};

}  // namespace sure_smartie::display
