#include "sure_smartie/engine/TemplateEngine.hpp"

#include <cctype>

namespace sure_smartie::engine {

core::Frame TemplateEngine::render(const core::ScreenDefinition& screen,
                                   const core::MetricMap& metrics,
                                   const core::DisplayGeometry& geometry) const {
  core::Frame frame;
  frame.reserve(geometry.rows);

  for (std::size_t row = 0; row < geometry.rows; ++row) {
    const std::string source =
        row < screen.lines.size() ? screen.lines[row] : std::string{};
    frame.push_back(fitToWidth(renderLine(source, metrics), geometry.cols));
  }

  return frame;
}

std::string TemplateEngine::fitToWidth(std::string text, std::size_t width) {
  for (char& symbol : text) {
    if (std::iscntrl(static_cast<unsigned char>(symbol))) {
      symbol = ' ';
    }
  }

  if (text.size() > width) {
    text.resize(width);
  } else if (text.size() < width) {
    text.resize(width, ' ');
  }

  return text;
}

std::string TemplateEngine::renderLine(const std::string& line,
                                       const core::MetricMap& metrics) {
  std::string output;
  output.reserve(line.size());

  for (std::size_t index = 0; index < line.size(); ++index) {
    if (line[index] != '{') {
      output.push_back(line[index]);
      continue;
    }

    const auto end = line.find('}', index + 1);
    if (end == std::string::npos) {
      output.push_back(line[index]);
      continue;
    }

    const auto key = line.substr(index + 1, end - index - 1);
    const auto metric = metrics.find(key);
    output.append(metric != metrics.end() ? metric->second : "-");
    index = end;
  }

  return output;
}

}  // namespace sure_smartie::engine
