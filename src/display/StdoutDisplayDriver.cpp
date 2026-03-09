#include "sure_smartie/display/StdoutDisplayDriver.hpp"

#include <iostream>

#include "sure_smartie/engine/TemplateEngine.hpp"

namespace sure_smartie::display {
namespace {

char printableGlyph(char symbol) {
  switch (static_cast<unsigned char>(symbol)) {
    case 1:
      return '.';
    case 2:
      return ':';
    case 3:
      return '=';
    case 4:
      return '#';
    case 5:
      return '@';
    default:
      return symbol;
  }
}

std::string toPrintable(std::string line) {
  for (char& symbol : line) {
    symbol = printableGlyph(symbol);
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
    std::cout << toPrintable(engine::TemplateEngine::fitToWidth(line, geometry_.cols))
              << '\n';
  }
}

void StdoutDisplayDriver::release() {}

void StdoutDisplayDriver::setBacklight([[maybe_unused]] bool on) {}

void StdoutDisplayDriver::setContrast([[maybe_unused]] std::uint8_t value) {}

void StdoutDisplayDriver::setBrightness([[maybe_unused]] std::uint8_t value) {}

void StdoutDisplayDriver::uploadCustomCharacter(
    [[maybe_unused]] std::uint8_t index,
    [[maybe_unused]] const std::array<std::uint8_t, 8>& pattern) {}

}  // namespace sure_smartie::display
