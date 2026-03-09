#pragma once

#include <string>

#include "sure_smartie/core/Types.hpp"

namespace sure_smartie::engine {

class TemplateEngine {
 public:
  core::Frame render(const core::ScreenDefinition& screen,
                     const core::MetricMap& metrics,
                     const core::DisplayGeometry& geometry) const;

  static std::string fitToWidth(std::string text, std::size_t width);

 private:
  static std::string renderBar(const std::string& metric_key,
                               std::size_t width,
                               double max_value,
                               const core::MetricMap& metrics);
  static std::string renderLine(const std::string& line,
                                const core::MetricMap& metrics);
};

}  // namespace sure_smartie::engine
