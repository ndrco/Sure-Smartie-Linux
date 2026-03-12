#include "sure_smartie/engine/TemplateEngine.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <optional>

namespace sure_smartie::engine {
namespace {

std::vector<std::string> split(const std::string& value, char delimiter) {
  std::vector<std::string> parts;
  std::string current;

  for (char symbol : value) {
    if (symbol == delimiter) {
      parts.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(symbol);
  }

  parts.push_back(current);
  return parts;
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t");
  return value.substr(first, last - first + 1);
}

std::optional<double> parseNumber(const std::string& value) {
  try {
    return std::stod(value);
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<int> parseInt(const std::string& value) {
  int parsed = 0;
  const auto trimmed = trim(value);
  const auto* begin = trimmed.data();
  const auto* end = trimmed.data() + trimmed.size();
  const auto result = std::from_chars(begin, end, parsed);
  if (result.ec != std::errc{} || result.ptr != end) {
    return std::nullopt;
  }
  return parsed;
}

}  // namespace

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
    const auto code = static_cast<unsigned char>(symbol);
    if (std::iscntrl(code) && (code == 0 || code > 7)) {
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

std::optional<std::size_t> TemplateEngine::estimateRenderedWidth(
    const std::string& line) {
  std::size_t width = 0;

  for (std::size_t index = 0; index < line.size(); ++index) {
    if (line[index] != '{') {
      ++width;
      continue;
    }

    const auto end = line.find('}', index + 1);
    if (end == std::string::npos) {
      return std::nullopt;
    }

    const auto key = line.substr(index + 1, end - index - 1);
    if (key.rfind("bar:", 0) == 0) {
      const auto arguments = split(key.substr(4), ',');
      if (arguments.size() < 2 || arguments.size() > 3) {
        return std::nullopt;
      }

      const auto metric_key = trim(arguments[0]);
      const auto bar_width = parseInt(arguments[1]);
      const auto max_value = arguments.size() > 2
                                 ? parseNumber(trim(arguments[2]))
                                 : std::optional<double>{100.0};

      if (metric_key.empty() || !bar_width.has_value() || *bar_width <= 0 ||
          !max_value.has_value() || *max_value <= 0.0) {
        return std::nullopt;
      }

      width += static_cast<std::size_t>(*bar_width);
    } else if (key.rfind("at:", 0) == 0) {
      const auto column = parseInt(key.substr(3));
      if (!column.has_value() || *column <= 0) {
        return std::nullopt;
      }

      width = std::max(width, static_cast<std::size_t>(*column - 1));
    } else if (key.empty()) {
      return std::nullopt;
    } else {
      width += 1;
    }

    index = end;
  }

  return width;
}

std::string TemplateEngine::renderBar(const std::string& metric_key,
                                      std::size_t width,
                                      double max_value,
                                      const core::MetricMap& metrics) {
  std::string bar(width, ' ');
  const auto metric = metrics.find(metric_key);
  if (metric == metrics.end() || width == 0 || max_value <= 0.0) {
    return bar;
  }

  const auto value = parseNumber(metric->second);
  if (!value.has_value()) {
    return bar;
  }

  const auto clamped_value = std::clamp(*value, 0.0, max_value);
  const auto total_steps = static_cast<int>(width * 5);
  const auto filled_steps = static_cast<int>(
      std::lround((clamped_value / max_value) * static_cast<double>(total_steps)));

  for (std::size_t index = 0; index < width; ++index) {
    const int cell_fill = std::clamp(filled_steps - static_cast<int>(index * 5), 0, 5);
    if (cell_fill > 0) {
      bar[index] = static_cast<char>(cell_fill);
    }
  }

  return bar;
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
    if (key.rfind("bar:", 0) == 0) {
      const auto arguments = split(key.substr(4), ',');
      std::string replacement = "-";

      if (arguments.size() >= 2 && arguments.size() <= 3) {
        const auto metric_key =
            arguments.empty() ? std::string{} : trim(arguments[0]);
        const auto width = parseInt(arguments[1]);
        const auto max_value = arguments.size() > 2
                                   ? parseNumber(trim(arguments[2]))
                                   : std::optional<double>{100.0};

        if (!metric_key.empty() && width.has_value() && *width > 0 &&
            max_value.has_value() && *max_value > 0.0) {
          replacement = renderBar(
              metric_key,
              static_cast<std::size_t>(*width),
              *max_value,
              metrics);
        }
      }

      output.append(replacement);
      index = end;
      continue;
    } else if (key.rfind("at:", 0) == 0) {
      const auto column = parseInt(key.substr(3));
      if (column.has_value() && *column > 0) {
        const auto target = static_cast<std::size_t>(*column - 1);
        if (output.size() < target) {
          output.resize(target, ' ');
        }
      }
      index = end;
      continue;
    }

    const auto metric = metrics.find(trim(key));
    output.append(metric != metrics.end() ? metric->second : "-");
    index = end;
  }

  return output;
}

}  // namespace sure_smartie::engine
