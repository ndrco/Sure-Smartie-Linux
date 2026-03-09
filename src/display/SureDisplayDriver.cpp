#include "sure_smartie/display/SureDisplayDriver.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

namespace sure_smartie::display {
namespace {

std::uint8_t clampProtocolByte(std::uint8_t value) {
  return static_cast<std::uint8_t>(std::clamp<int>(value, 1, 254));
}

}  // namespace

void SureDisplayDriver::uploadBuiltInGlyphs() {
  static constexpr std::array<std::array<std::uint8_t, 8>, 5> kBarGlyphs{{
      {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
      {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},
      {0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C},
      {0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E},
      {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},
  }};

  for (std::size_t index = 0; index < kBarGlyphs.size(); ++index) {
    uploadCustomCharacter(static_cast<std::uint8_t>(index + 1), kBarGlyphs[index]);
  }
}

SureDisplayDriver::SureDisplayDriver(std::string device,
                                     int baudrate,
                                     core::DisplayGeometry geometry,
                                     bool backlight,
                                     std::uint8_t contrast,
                                     std::uint8_t brightness)
    : serial_(std::move(device), baudrate),
      geometry_(geometry),
      backlight_(backlight),
      contrast_(clampProtocolByte(contrast)),
      brightness_(clampProtocolByte(brightness)) {}

core::DisplayGeometry SureDisplayDriver::geometry() const { return geometry_; }

void SureDisplayDriver::initialize() {
  if (initialized_) {
    return;
  }

  serial_.open();
  std::this_thread::sleep_for(std::chrono::milliseconds{200});

  // Match the known-good Python probe sequence first.
  setBacklight(backlight_);
  setContrast(contrast_);
  if (backlight_) {
    setBrightness(brightness_);
  }
  uploadBuiltInGlyphs();

  initialized_ = true;
}

void SureDisplayDriver::render(const core::Frame& frame) {
  initialize();

  for (std::size_t row = 0; row < geometry_.rows; ++row) {
    writeLine(row + 1, row < frame.size() ? frame[row] : "");
  }
}

void SureDisplayDriver::setBacklight(bool on) {
  if (!serial_.isOpen()) {
    throw std::runtime_error("Display is not initialized");
  }

  backlight_ = on;
  if (on) {
    writeBytes({0xFE, 0x42, 0x00});
    setBrightness(brightness_);
    return;
  }

  writeBytes({0xFE, 0x46});
}

void SureDisplayDriver::setContrast(std::uint8_t value) {
  if (!serial_.isOpen()) {
    throw std::runtime_error("Display is not initialized");
  }

  contrast_ = clampProtocolByte(value);
  writeBytes({0xFE, 0x50, contrast_});
}

void SureDisplayDriver::setBrightness(std::uint8_t value) {
  if (!serial_.isOpen()) {
    throw std::runtime_error("Display is not initialized");
  }

  brightness_ = clampProtocolByte(value);
  if (backlight_) {
    writeBytes({0xFE, 0x98, brightness_});
  }
}

void SureDisplayDriver::uploadCustomCharacter(
    std::uint8_t index, const std::array<std::uint8_t, 8>& pattern) {
  if (!serial_.isOpen()) {
    throw std::runtime_error("Display is not initialized");
  }
  if (index > 7) {
    throw std::invalid_argument("Custom character index must be in range 0..7");
  }

  std::vector<std::uint8_t> bytes{0xFE, 0x4E, index};
  bytes.insert(bytes.end(), pattern.begin(), pattern.end());
  serial_.write(bytes);
}

std::string SureDisplayDriver::sanitizeText(std::string text, std::size_t width) {
  if (text.size() > width) {
    text.resize(width);
  }

  for (char& symbol : text) {
    const auto code = static_cast<unsigned char>(symbol);
    if ((code < 0x20 && code != 0x01 && code != 0x02 && code != 0x03 &&
         code != 0x04 && code != 0x05 && code != 0x06 && code != 0x07) ||
        code > 0x7E) {
      symbol = '?';
    }
  }

  text.resize(width, ' ');
  return text;
}

void SureDisplayDriver::writeLine(std::size_t row, const std::string& text) {
  if (row == 0 || row > geometry_.rows) {
    throw std::out_of_range("Display row is out of range");
  }

  const auto payload = sanitizeText(text, geometry_.cols);
  std::vector<std::uint8_t> bytes{0xFE, 0x47, 0x01, static_cast<std::uint8_t>(row)};
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  serial_.write(bytes);
}

void SureDisplayDriver::writeBytes(std::initializer_list<std::uint8_t> bytes) {
  serial_.write(bytes);
}

}  // namespace sure_smartie::display
