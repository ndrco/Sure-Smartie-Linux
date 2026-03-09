#include "sure_smartie/display/DisplayFactory.hpp"

#include <memory>
#include <stdexcept>

#include "sure_smartie/display/StdoutDisplayDriver.hpp"
#include "sure_smartie/display/SureDisplayDriver.hpp"

namespace sure_smartie::display {

std::unique_ptr<IDisplay> createDisplay(const core::AppConfig& config,
                                        const core::RuntimeOptions& options) {
  const core::DisplayGeometry geometry{
      .cols = config.display.cols,
      .rows = config.display.rows,
  };

  if (options.force_stdout_display || config.display.type == "stdout") {
    return std::make_unique<StdoutDisplayDriver>(geometry);
  }

  if (config.display.type == "sure") {
    return std::make_unique<SureDisplayDriver>(config.device,
                                               config.baudrate,
                                               geometry,
                                               config.display.backlight,
                                               config.display.contrast,
                                               config.display.brightness);
  }

  throw std::invalid_argument("Unknown display type: " + config.display.type);
}

}  // namespace sure_smartie::display
