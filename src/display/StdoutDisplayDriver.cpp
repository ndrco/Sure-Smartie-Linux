#include "sure_smartie/display/StdoutDisplayDriver.hpp"

#include <iostream>

#include "sure_smartie/engine/TemplateEngine.hpp"

namespace sure_smartie::display {

StdoutDisplayDriver::StdoutDisplayDriver(core::DisplayGeometry geometry)
    : geometry_(geometry) {}

core::DisplayGeometry StdoutDisplayDriver::geometry() const { return geometry_; }

void StdoutDisplayDriver::initialize() {}

void StdoutDisplayDriver::render(const core::Frame& frame) {
  std::cout << "----------------------\n";
  for (std::size_t row = 0; row < geometry_.rows; ++row) {
    const std::string line =
        row < frame.size() ? frame[row] : std::string(geometry_.cols, ' ');
    std::cout
        << engine::TemplateEngine::fitToWidth(line, geometry_.cols)
        << '\n';
  }
}

void StdoutDisplayDriver::setBacklight([[maybe_unused]] bool on) {}

void StdoutDisplayDriver::setContrast([[maybe_unused]] std::uint8_t value) {}

void StdoutDisplayDriver::setBrightness([[maybe_unused]] std::uint8_t value) {}

void StdoutDisplayDriver::uploadCustomCharacter(
    [[maybe_unused]] std::uint8_t index,
    [[maybe_unused]] const std::array<std::uint8_t, 8>& pattern) {}

}  // namespace sure_smartie::display
