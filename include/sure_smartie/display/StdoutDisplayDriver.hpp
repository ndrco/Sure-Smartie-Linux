#pragma once

#include <array>
#include <cstdint>

#include "sure_smartie/display/IDisplay.hpp"

namespace sure_smartie::display {

class StdoutDisplayDriver : public IDisplay {
 public:
  explicit StdoutDisplayDriver(core::DisplayGeometry geometry);

  core::DisplayGeometry geometry() const override;
  void initialize() override;
  void render(const core::Frame& frame) override;
  void release() override;
  void setBacklight(bool on) override;
  void setContrast(std::uint8_t value) override;
  void setBrightness(std::uint8_t value) override;
 void uploadCustomCharacter(std::uint8_t index,
                             const std::array<std::uint8_t, 8>& pattern) override;

 private:
  core::DisplayGeometry geometry_;
  std::array<bool, core::kGlyphSlotCount> glyph_active_{};
  std::array<std::array<std::uint8_t, 8>, core::kGlyphSlotCount> glyph_patterns_{};
};

}  // namespace sure_smartie::display
