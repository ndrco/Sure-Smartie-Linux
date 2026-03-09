#pragma once

#include <string_view>

#include "sure_smartie/core/Types.hpp"
#include "sure_smartie/engine/TemplateEngine.hpp"

namespace sure_smartie::core {

class PreviewFrameRenderer {
 public:
  Frame render(const ScreenDefinition& screen,
               const DisplayGeometry& geometry,
               const MetricMap& metrics) const;

  Frame renderScreen(const AppConfig& config,
                     const MetricMap& metrics,
                     std::size_t screen_index) const;

  Frame renderScreen(const AppConfig& config,
                     const MetricMap& metrics,
                     std::string_view screen_name) const;

 private:
  engine::TemplateEngine template_engine_;
};

}  // namespace sure_smartie::core
