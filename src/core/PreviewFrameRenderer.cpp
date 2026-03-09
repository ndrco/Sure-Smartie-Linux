#include "sure_smartie/core/PreviewFrameRenderer.hpp"

#include <stdexcept>

namespace sure_smartie::core {

Frame PreviewFrameRenderer::render(const ScreenDefinition& screen,
                                   const DisplayGeometry& geometry,
                                   const MetricMap& metrics) const {
  return template_engine_.render(screen, metrics, geometry);
}

Frame PreviewFrameRenderer::renderScreen(const AppConfig& config,
                                         const MetricMap& metrics,
                                         std::size_t screen_index) const {
  if (screen_index >= config.screens.size()) {
    throw std::out_of_range("screen index is out of range");
  }

  return render(config.screens[screen_index], normalizedGeometry(config.display), metrics);
}

Frame PreviewFrameRenderer::renderScreen(const AppConfig& config,
                                         const MetricMap& metrics,
                                         std::string_view screen_name) const {
  for (const auto& screen : config.screens) {
    if (screen.name == screen_name) {
      return render(screen, normalizedGeometry(config.display), metrics);
    }
  }

  throw std::invalid_argument("screen name not found");
}

}  // namespace sure_smartie::core
