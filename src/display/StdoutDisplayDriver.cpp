#include "sure_smartie/display/StdoutDisplayDriver.hpp"

#include <algorithm>
#include <iostream>
#include <string_view>

#include "sure_smartie/core/GlyphSupport.hpp"
#include "sure_smartie/engine/TemplateEngine.hpp"

namespace sure_smartie::display {
namespace {

using ProtocolGlyphPattern = std::array<std::uint8_t, 8>;

bool patternsEqual(const ProtocolGlyphPattern& left, const core::GlyphPattern& right) {
  for (std::size_t row = 0; row < left.size(); ++row) {
    if (left[row] != static_cast<std::uint8_t>(std::clamp(right[row], 0, 0x1F))) {
      return false;
    }
  }
  return true;
}

char printableCustomGlyph(const ProtocolGlyphPattern& pattern) {
  if (patternsEqual(pattern, core::barGlyphPattern(0))) {
    return '_';
  }
  if (patternsEqual(pattern, core::barGlyphPattern(1))) {
    return '.';
  }
  if (patternsEqual(pattern, core::barGlyphPattern(2))) {
    return ':';
  }
  if (patternsEqual(pattern, core::barGlyphPattern(3))) {
    return '=';
  }
  if (patternsEqual(pattern, core::barGlyphPattern(4))) {
    return '#';
  }
  if (patternsEqual(pattern, core::barGlyphPattern(5))) {
    return '@';
  }

  int filled_pixels = 0;
  for (const auto row : pattern) {
    for (int column = 0; column < 5; ++column) {
      if ((row & (1 << column)) != 0) {
        ++filled_pixels;
      }
    }
  }

  constexpr std::string_view kRamp = ".:-=+*#%@";
  const auto bucket = static_cast<std::size_t>(
      (filled_pixels * static_cast<int>(kRamp.size() - 1)) / 40);
  return kRamp[bucket];
}

char printableGlyph(char symbol,
                    const std::array<bool, core::kGlyphSlotCount>& glyph_active,
                    const std::array<ProtocolGlyphPattern, core::kGlyphSlotCount>& glyph_patterns) {
  const auto code = static_cast<unsigned char>(symbol);
  if (code < glyph_active.size() && glyph_active[code]) {
    return printableCustomGlyph(glyph_patterns[code]);
  }

  return symbol;
}

std::string toPrintable(
    std::string line,
    const std::array<bool, core::kGlyphSlotCount>& glyph_active,
    const std::array<ProtocolGlyphPattern, core::kGlyphSlotCount>& glyph_patterns) {
  for (char& symbol : line) {
    symbol = printableGlyph(symbol, glyph_active, glyph_patterns);
  }
  return line;
}

}  // namespace

StdoutDisplayDriver::StdoutDisplayDriver(core::DisplayGeometry geometry)
    : geometry_(geometry) {}

core::DisplayGeometry StdoutDisplayDriver::geometry() const { return geometry_; }

void StdoutDisplayDriver::initialize() {}

void StdoutDisplayDriver::render(const core::Frame& frame) {
  std::cout << "----------------------\n";
  for (std::size_t row = 0; row < geometry_.rows; ++row) {
    const std::string line =
        row < frame.size() ? frame[row] : std::string(geometry_.cols, ' ');
    std::cout << toPrintable(engine::TemplateEngine::fitToWidth(line, geometry_.cols),
                             glyph_active_,
                             glyph_patterns_)
              << '\n';
  }
}

void StdoutDisplayDriver::release() {}

void StdoutDisplayDriver::setBacklight([[maybe_unused]] bool on) {}

void StdoutDisplayDriver::setContrast([[maybe_unused]] std::uint8_t value) {}

void StdoutDisplayDriver::setBrightness([[maybe_unused]] std::uint8_t value) {}

void StdoutDisplayDriver::uploadCustomCharacter(
    std::uint8_t index,
    const std::array<std::uint8_t, 8>& pattern) {
  if (index >= glyph_patterns_.size()) {
    return;
  }

  glyph_active_[index] = true;
  glyph_patterns_[index] = pattern;
}

}  // namespace sure_smartie::display
