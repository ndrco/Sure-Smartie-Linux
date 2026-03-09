#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "sure_smartie/display/IDisplay.hpp"
#include "sure_smartie/serial/SerialPort.hpp"

namespace sure_smartie::display {

class SureDisplayDriver : public IDisplay {
 public:
  SureDisplayDriver(std::string device,
                    int baudrate,
                    core::DisplayGeometry geometry,
                    bool backlight,
                    int contrast,
                    int brightness);

  core::DisplayGeometry geometry() const override;
  void initialize() override;
  void render(const core::Frame& frame) override;
  void setBacklight(bool on) override;
  void setContrast(std::uint8_t value) override;
  void setBrightness(std::uint8_t value) override;
  void uploadCustomCharacter(std::uint8_t index,
                             const std::array<std::uint8_t, 8>& pattern) override;

 private:
  void uploadBuiltInGlyphs();
  static std::string sanitizeText(std::string text, std::size_t width);
  void writeLine(std::size_t row, const std::string& text);
  void writeBytes(std::initializer_list<std::uint8_t> bytes);

  serial::SerialPort serial_;
  core::DisplayGeometry geometry_;
  bool backlight_;
  std::uint8_t contrast_;
  std::uint8_t brightness_;
  bool initialized_{false};
};

}  // namespace sure_smartie::display
