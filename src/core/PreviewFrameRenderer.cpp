#include "sure_smartie/core/PreviewFrameRenderer.hpp"

#include <stdexcept>

namespace sure_smartie::core {

RenderedFrame PreviewFrameRenderer::renderDetailed(
    const ScreenDefinition& screen,
    const DisplayGeometry& geometry,
    const MetricMap& metrics,
    const std::vector<CustomGlyphDefinition>& custom_glyphs) const {
  return template_engine_.renderDetailed(screen, metrics, geometry, custom_glyphs);
}

Frame PreviewFrameRenderer::render(const ScreenDefinition& screen,
                                   const DisplayGeometry& geometry,
                                   const MetricMap& metrics) const {
  return renderDetailed(screen, geometry, metrics, {}).frame;
}

RenderedFrame PreviewFrameRenderer::renderScreenDetailed(
    const AppConfig& config,
    const MetricMap& metrics,
    std::size_t screen_index) const {
  if (screen_index >= config.screens.size()) {
    throw std::out_of_range("screen index is out of range");
  }

  return renderDetailed(config.screens[screen_index],
                        normalizedGeometry(config.display),
                        metrics,
                        config.custom_glyphs);
}

Frame PreviewFrameRenderer::renderScreen(const AppConfig& config,
                                         const MetricMap& metrics,
                                         std::size_t screen_index) const {
  return renderScreenDetailed(config, metrics, screen_index).frame;
}

RenderedFrame PreviewFrameRenderer::renderScreenDetailed(
    const AppConfig& config,
    const MetricMap& metrics,
    std::string_view screen_name) const {
  for (const auto& screen : config.screens) {
    if (screen.name == screen_name) {
      return renderDetailed(screen,
                            normalizedGeometry(config.display),
                            metrics,
                            config.custom_glyphs);
    }
  }

  throw std::invalid_argument("screen name not found");
}

Frame PreviewFrameRenderer::renderScreen(const AppConfig& config,
                                         const MetricMap& metrics,
                                         std::string_view screen_name) const {
  return renderScreenDetailed(config, metrics, screen_name).frame;
}

}  // namespace sure_smartie::core
